/* my_module.c: Example char device module.
*
*/
/* Kernel Programming */
#define MODULE
#define LINUX
#define __KERNEL__

#include <linux/kernel.h>  	
#include <linux/module.h>
#include <linux/fs.h>       		
#include <asm/uaccess.h>
#include <linux/errno.h>  
#include <linux/slab.h>

#include "my_module.h"

#define MY_DEVICE "s19_device"
#define BUFF_SIZE 4096

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anonymous");

struct buff_list;

/* globals */
int my_major = 0; /* will hold the major # of my device driver */
struct buff_list* head = NULL;


/* definitions of types and structs*/

// stats for the FSMs
typedef enum Modes
{
	my_NA = 0, my_RO = 1, my_WO = 2, my_RW = 3
}Modes;

// buffers related to each device file
typedef struct buff_data {
	char buff[BUFF_SIZE];
	int curr_buff_size;
	int lseek;
	unsigned long seed;
	Modes my_mode;
	int open;
}buff_data;

// linked list to hold the buffers
typedef struct buff_list {
	int key;
	struct buff_data* my_buff;
	struct buff_list* p_next;
}buff_list;

// overload for device driver operations
struct file_operations my_fops = {
	.open = my_open,
	.release = my_release,
	.read = my_read,
	.write = my_write,
	.ioctl = my_ioctl
};

buff_list* mk_node(int key);
buff_list* find_node(int key);
void add_node(buff_list* new_node);
void rm_node(int key);
uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed);


// init func, register the device
int init_module(void)
{
	my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

	if (my_major < 0)
	{
		printk(KERN_WARNING "can't get dynamic major\n");
		return my_major;
	}
	printk("Major number: %d\n", my_major);
	return 0;
}

// unregister the device, and all of the linked list
void cleanup_module(void)
{
	unregister_chrdev(my_major, MY_DEVICE);
	printk("Erasing all buffers\n");
	int i = 0;
	while (head != NULL)
	{
		buff_list* tmp = head->p_next;
		kfree(head->my_buff);
		kfree(head);
		head = tmp;
	}
	printk("removed %d buffers\n", ++i);
	return;
}

// open func for the device, check if a buffer exists in the linked list.
// create one if not and update the variables of the buffer (read/write)
int my_open(struct inode *inode, struct file *filp)
{
	unsigned int key = MINOR(inode->i_rdev);
	buff_list* node = find_node(key);
	if (node == NULL)
	{   // first device file open, create node.
		printk("first device file open for minor %d, creating node\n", key);
		node = mk_node(key);
		add_node(node);
	}
	else // for debug help
		printk("device file node for minor %d already exists\n", key);
	if (node->my_buff->open) {
		printk("device file for minor %d is currently opened\n", key);
		return 0;
	}
	// update the node to indicate the file is open
	node->my_buff->open = 1;
	// update device open mode (read/write)
	node->my_buff->my_mode = my_NA;
	if (filp->f_mode & FMODE_READ)
		node->my_buff->my_mode += my_RO;
	if (filp->f_mode & FMODE_WRITE)
		node->my_buff->my_mode += my_WO;
	// keep node in private_data for future use
	filp->private_data = node;
	return 0;
}

// device close function, update the buffer node to closed status
int my_release(struct inode *inode, struct file *filp)
{
	((buff_list*)filp->private_data)->my_buff->open = 0;
	((buff_list*)filp->private_data)->my_buff->my_mode = my_NA;
	return 0;
}



//
// Do read operation.
// Return number of bytes read.
ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	buff_list* node = (buff_list*)filp->private_data;
	if ((node->my_buff->my_mode & 0x1) == 0) // read possible if mode is 1 or 3 (LSB = 1)
	{
		printk("Tried to read from device file that is not in Read mode, DAMN\n");
		return 0;
	}
	// make sure we don't overflow (take all the data if higher amount was requested)
	int dataLeft = (node->my_buff->curr_buff_size) - (node->my_buff->lseek);
	if (count > dataLeft) count = dataLeft;
	// if there is nothing to read we can just return now.
	if (count == 0) return 0;
	printk("creating output data\n");
	// create the output data
	char* res = (char*)kmalloc(count+ sizeof(uint32_t), GFP_KERNEL);
	char* key = (node->my_buff->buff) + (node->my_buff->lseek);
	uint32_t hash = murmur3_32(key, count, node->my_buff->seed);
	printk("done hashing\n");
	int i;
	// Concat the hash to the string
	char* hash_casted = (char*)&hash;
	for (i = 0; i<sizeof(uint32_t); i++) {
		res[i] = hash_casted[i];
	}

	for (i = 0; i<count; i++) {
		res[i + sizeof(uint32_t)] = key[i];
	}

	res[sizeof(uint32_t) + count] = '\0';
	// Done concating, copy to user
	if (copy_to_user(buf, res, count + 4))
	{
		printk("Error while copying to user space\n");
		kfree(res);
		return -ENOMEM;
	}
	node->my_buff->lseek += count;
	printk("done copying to user, lseek = %d\n", node->my_buff->lseek);
	count += sizeof(hash);
	kfree(res);
	printk("read complete, Hash value is: %u, Return value: %u\n",hash,count);
	return count;
}


//
// Do write operation.
// Return number of bytes written.
ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	buff_list* node = (buff_list*)filp->private_data;
	if ((node->my_buff->my_mode & 0x2) == 0) // read possible if mode is 2 or 3 (MSB = 1)
	{
		printk("Tried to write to device file that is not in write mode, DAMN\n");
		return 0;
	}
	// make sure write request isn't bad
	int csize = node->my_buff->curr_buff_size;
	if (count > (BUFF_SIZE - csize))
	{
		printk("Write overflow, returning -ENOMEM\n");
		return -ENOMEM;
	}
	if (count == 0)
	{
		printk("Can't write string of len 0 (OBVIOUSLY GEEZ)\n");
		return -EINVAL;
	}
	//write is good, get the data from the user
	if (copy_from_user((node->my_buff->buff + csize), buf, count))
	{
		printk("Error while copying from user space\n");
		return -ENOMEM;
	}
	node->my_buff->curr_buff_size += count;
	printk("\nDone Writing, curr_buf_size =%d\n", node->my_buff->curr_buff_size);
	return count;
}

// handler for ioctl requests, parse input and perform
// the requested action (RESET,SEED,RESTART)
int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd)
	{
	case MY_RESET:
		printk("RESET is on\n");
		if (filp->private_data == NULL) return 0;
		// set read and write to the start of the buffer to invalidate current data
		((buff_list*)filp->private_data)->my_buff->lseek = 0;
		((buff_list*)filp->private_data)->my_buff->curr_buff_size = 0;
		break;
	case MY_RESTART:
		printk("RESTART is on\n");
		if (filp->private_data == NULL) return 0;
		// set read to the start of the buffer
		((buff_list*)filp->private_data)->my_buff->lseek = 0;
		break;
	case MY_SET_SEED:
		printk("SEED changed from %lu to %lu\n", ((buff_list*)filp->private_data)->my_buff->seed, arg);
		if (filp->private_data == NULL) return 0;
		// set the seed for this device file
		((buff_list*)filp->private_data)->my_buff->seed = arg;
		break;
	default:
		return -ENOTTY;
	}
	return 0;
}

uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed) {
	uint32_t h = seed;
	if (len > 3) {
		const uint32_t* key_x4 = (const uint32_t*)key;
		size_t i = len >> 2;
		do {
			uint32_t k = *key_x4++;
			k *= 0xcc9e2d51;
			k = (k << 15) | (k >> 17);
			k *= 0x1b873593;
			h ^= k;
			h = (h << 13) | (h >> 19);
			h = (h * 5) + 0xe6546b64;
		} while (--i);
		key = (const uint8_t*)key_x4;
	}
	if (len & 3) {
		size_t i = len & 3;
		uint32_t k = 0;
		key = &key[i - 1];
		do {
			k <<= 8;
			k |= *key--;
		} while (--i);
		k *= 0xcc9e2d51;
		k = (k << 15) | (k >> 17);
		k *= 0x1b873593;
		h ^= k;
	}
	h ^= len;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

// get the minor of a device file, 
// and search for it's node in the linked list
buff_list* find_node(int key) {
	buff_list* curr_node = head;
	while (curr_node)
	{
		if (curr_node->key == key) break;
		curr_node = curr_node->p_next;
	}
	return curr_node;
}

// add the given node to the head of the linked list
void add_node(buff_list* new_node) {
	buff_list* temp = head;
	head = new_node;
	new_node->p_next = temp;
}

// remove the node for the given minor number from the linked list
void rm_node(int key) {
	if (find_node(key) == NULL) return;
	//the node exists
	buff_list* itr = head;
	// handle the case where we need to remove the head of the list
	if (itr->key == key)
	{
		head = itr->p_next;
		kfree(itr->my_buff);
		kfree(itr);
		return;
	}
	// the key does not correspond to the head of the list
	// search the rest of the list and remove the node
	while (itr->p_next->key != key)
	{
		itr = itr->p_next;
	}
	buff_list* temp = itr->p_next;
	itr->p_next = temp->p_next;
	kfree(temp->my_buff);
	kfree(temp);
}

// create a new node for the given minor number
buff_list* mk_node(int key) {
	buff_list* new_node = (buff_list*)kmalloc(sizeof(buff_list), GFP_KERNEL);
	new_node->my_buff = (buff_data*)kmalloc(sizeof(buff_data), GFP_KERNEL);
	new_node->key = key;
	new_node->p_next = NULL;
	new_node->my_buff->my_mode = 0;
	new_node->my_buff->seed = 0;
	new_node->my_buff->curr_buff_size = 0;
	new_node->my_buff->open = 0;
	new_node->my_buff->lseek = 0;
	return new_node;
}


