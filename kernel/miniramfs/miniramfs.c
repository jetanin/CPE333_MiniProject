// SPDX-License-Identifier: GPL-2.0
/*
 * Educational RAM filesystem using Linux VFS helpers.
 * File contents live in the page cache; there is no disk backing.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/pagemap.h>
#include <linux/err.h>
#include "compat.h"

#define MP_MAGIC 0x4D505232
#define MP_MAX_FILE_SIZE (16LL * 1024 * 1024)

static const struct inode_operations mp_directory_ops;

static const struct file_operations mp_file_ops = {
    .owner      = THIS_MODULE,
    .read_iter  = generic_file_read_iter,
    .write_iter = generic_file_write_iter,
    .llseek     = generic_file_llseek,
};

static const struct inode_operations mp_file_inode_ops = {
    .setattr = simple_setattr,
    .getattr = simple_getattr,
};

/* Unhashed RAM-only inodes can be discarded when no longer referenced. */
static int mp_drop_inode(struct inode *inode)
{
    return 1;
}

static const struct super_operations mp_super_ops = {
    .statfs     = simple_statfs,
    .drop_inode = mp_drop_inode,
};

static struct inode *mp_allocate_inode(
    struct super_block *sb,
    struct mnt_idmap *idmap,
    const struct inode *parent,
    umode_t mode)
{
    struct inode *node = new_inode(sb);

    if (!node)
        return NULL;

    node->i_ino = get_next_ino();
    inode_init_owner(idmap, node, parent, mode);
    simple_inode_init_ts(node);

    if (S_ISDIR(mode)) {
        node->i_op = &mp_directory_ops;
        node->i_fop = &simple_dir_operations;
        set_nlink(node, 2);
    } else {
        node->i_op = &mp_file_inode_ops;
        node->i_fop = &mp_file_ops;
        node->i_mapping->a_ops = &ram_aops;
        mapping_set_gfp_mask(node->i_mapping, GFP_HIGHUSER);
        mapping_set_unevictable(node->i_mapping);
    }

    return node;
}

static int mp_add_entry(
    struct mnt_idmap *idmap,
    struct inode *parent,
    struct dentry *entry,
    umode_t mode)
{
    struct inode *node;

    node = mp_allocate_inode(parent->i_sb, idmap, parent, mode);
    if (!node)
        return -ENOMEM;

#if MP_HAS_PERSISTENT_DENTRY
    d_make_persistent(entry, node);
#else
    d_instantiate(entry, node);
    dget(entry);
#endif

    if (S_ISDIR(mode))
        inc_nlink(parent);

    inode_set_mtime_to_ts(parent, inode_set_ctime_current(parent));

    pr_info("miniramfs: created %pd (%s)\n",
            entry, S_ISDIR(mode) ? "directory" : "file");

    return 0;
}

static int mp_create(
    struct mnt_idmap *idmap,
    struct inode *parent,
    struct dentry *entry,
    umode_t mode,
    bool exclusive)
{
    return mp_add_entry(idmap, parent, entry, S_IFREG | mode);
}

#if MP_MKDIR_RETURNS_DENTRY
static struct dentry *mp_mkdir(
    struct mnt_idmap *idmap,
    struct inode *parent,
    struct dentry *entry,
    umode_t mode)
{
    int error = mp_add_entry(idmap, parent, entry, S_IFDIR | mode);
    return ERR_PTR(error);
}
#else
static int mp_mkdir(
    struct mnt_idmap *idmap,
    struct inode *parent,
    struct dentry *entry,
    umode_t mode)
{
    return mp_add_entry(idmap, parent, entry, S_IFDIR | mode);
}
#endif

static const struct inode_operations mp_directory_ops = {
    .lookup  = simple_lookup,
    .create  = mp_create,
    .mkdir   = mp_mkdir,
    .unlink  = simple_unlink,
    .rmdir   = simple_rmdir,
    .setattr = simple_setattr,
    .getattr = simple_getattr,
};

static int mp_fill_super(
    struct super_block *sb,
    struct fs_context *fc)
{
    struct inode *root;

    sb->s_magic = MP_MAGIC;
    sb->s_op = &mp_super_ops;
    sb->s_blocksize = PAGE_SIZE;
    sb->s_blocksize_bits = PAGE_SHIFT;
    sb->s_maxbytes = MP_MAX_FILE_SIZE;
    sb->s_time_gran = 1;

    root = mp_allocate_inode(
        sb, &nop_mnt_idmap, NULL, S_IFDIR | 0755);

    if (!root)
        return -ENOMEM;

    sb->s_root = d_make_root(root);
    if (!sb->s_root)
        return -ENOMEM;

    pr_info("miniramfs: new filesystem instance mounted\n");
    return 0;
}

static int mp_get_tree(struct fs_context *fc)
{
    return get_tree_nodev(fc, mp_fill_super);
}

static const struct fs_context_operations mp_context_ops = {
    .get_tree = mp_get_tree,
};

static int mp_init_context(struct fs_context *fc)
{
    fc->ops = &mp_context_ops;
    return 0;
}

static void mp_kill_super(struct super_block *sb)
{
    pr_info("miniramfs: destroying filesystem instance\n");
    kill_anon_super(sb);
}

static struct file_system_type mp_type = {
    .owner           = THIS_MODULE,
    .name            = "miniramfs",
    .init_fs_context = mp_init_context,
    .kill_sb         = mp_kill_super,
};

static int __init mp_load(void)
{
    int error = register_filesystem(&mp_type);

    if (!error)
        pr_info("miniramfs: module loaded\n");

    return error;
}

static void __exit mp_unload(void)
{
    unregister_filesystem(&mp_type);
    pr_info("miniramfs: module unloaded\n");
}

module_init(mp_load);
module_exit(mp_unload);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MiniProject2 Team");
MODULE_DESCRIPTION("Educational RAM filesystem");
MODULE_ALIAS_FS("miniramfs");
