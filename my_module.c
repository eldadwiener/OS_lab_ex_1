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

#include "my_module.h"

#define MY_DEVICE "s19_device"
#define BUFF_SIZE 4096

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anonymous");

struct buff_list;

/* globals */
int my_major = 0; /* will hold the major # of my device driver */
struct buff_list* head = NULL;

enum Modes
{
	my_RO = 0, my_WO = 1, my_RW = 2
};

struct buff_data {
	char buff[BUFF_SIZE];
	int curr_buff_size;
	int lseek;
	int seed;
	enum Modes my_mode;
};


struct buff_list {
	int key;
	struct buff_data* my_buff;
	struct buff_list* p_next;
};


struct file_operations my_fops = {
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .write = my_write,
    .ioctl = my_ioctl
};

struct buff_list* mk_node(enum Modes mode, int key);
struct buff_list* find_node(int key);
void add_node(struct buff_list* new_node);
void rm_node(int key);


int init_module(void)
{
    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

    if (my_major < 0)
    {
	printk(KERN_WARNING "can't get dynamic major\n");
	return my_major;
    }

    //
    // do_init();
    //
    return 0;
}


void cleanup_module(void)
{
    unregister_chrdev(my_major, MY_DEVICE);

    //
    // do clean_up();
    //
    return;
}


int my_open(struct inode *inode, struct file *filp)
{
    if (filp->f_mode & FMODE_READ)
    {
	//
	// handle read opening
	//
    }
    
    if (filp->f_mode & FMODE_WRITE)
    {
        //
        // handle write opening
        //
    }

    return 0;
}


int my_release(struct inode *inode, struct file *filp)
{
    if (filp->f_mode & FMODE_READ)
    {
	//
	// handle read closing
	// 
    }
    
    if (filp->f_mode & FMODE_WRITE)
    {
        //
        // handle write closing
        //
    }

    return 0;
}


ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    //
    // Do read operation.
    // Return number of bytes read.
    return 0; 
}


ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    //
    // Do write operation.
    // Return number of bytes written.
    return 0; 
}


int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch(cmd)
    {
    case MY_OP1:
	//
	// handle OP 1.
	//
	break;

    default:
	return -ENOTTY;
    }

    return 0;
}

uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed) {
  uint32_t h = seed;
  if (len > 3) {
    const uint32_t* key_x4 = (const uint32_t*) key;
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
    key = (const uint8_t*) key_x4;
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

struct buff_list* find_node(int key) {
	struct buff_list* curr_node = head;
	while (curr_node)
	{
		if (curr_node->key == key) break;
		curr_node = curr_node->p_next;
	}
	return curr_node;
}

void add_node(struct buff_list* new_node) {
	struct buff_list* temp = head;
	head = new_node;
	new_node->p_next = temp;
}

void rm_node(int key) {
	if (find_node(key) == NULL) return;
	//the node exist
	struct buff_list* itr = head;
	if (itr->key == key)
	{
		head = itr->p_next;
		kfree(itr);
		return;
	}
	while (itr->p_next->key != key)
	{
		itr = itr->p_next;
	}
	struct buff_list* temp = itr->p_next;
	itr->p_next = temp->p_next;
	kfree(temp);
}

struct buff_list* mk_node(enum Modes mode, int key) {
	struct buff_list* new_node = (struct buff_list*) kmalloc(sizeof(struct buff_list), GFP_KERNEL);
	new_node->key = key;
	new_node->p_next = NULL;
	new_node->my_buff->my_mode = mode;
	new_node->my_buff->seed = 0;
}
