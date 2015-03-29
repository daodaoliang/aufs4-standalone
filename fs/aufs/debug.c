/*
 * Copyright (C) 2005-2015 Junjiro R. Okajima
 *
 * This program, aufs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * debug print functions
 */

#include "aufs.h"

/* Returns 0, or -errno.  arg is in kp->arg. */
static int param_atomic_t_set(const char *val, const struct kernel_param *kp)
{
	int err, n;

	err = kstrtoint(val, 0, &n);
	if (!err) {
		if (n > 0)
			au_debug_on();
		else
			au_debug_off();
	}
	return err;
}

/* Returns length written or -errno.  Buffer is 4k (ie. be short!) */
static int param_atomic_t_get(char *buffer, const struct kernel_param *kp)
{
	atomic_t *a;

	a = kp->arg;
	return sprintf(buffer, "%d", atomic_read(a));
}

static struct kernel_param_ops param_ops_atomic_t = {
	.set = param_atomic_t_set,
	.get = param_atomic_t_get
	/* void (*free)(void *arg) */
};

atomic_t aufs_debug = ATOMIC_INIT(0);
MODULE_PARM_DESC(debug, "debug print");
module_param_named(debug, aufs_debug, atomic_t, S_IRUGO | S_IWUSR | S_IWGRP);

DEFINE_MUTEX(au_dbg_mtx);	/* just to serialize the dbg msgs */
char *au_plevel = KERN_DEBUG;
#define dpri(fmt, ...) do {					\
	if ((au_plevel						\
	     && strcmp(au_plevel, KERN_DEBUG))			\
	    || au_debug_test())					\
		printk("%s" fmt, au_plevel, ##__VA_ARGS__);	\
} while (0)

/* ---------------------------------------------------------------------- */

static int do_pri_inode(aufs_bindex_t bindex, struct inode *inode)
{
	if (!inode || IS_ERR(inode)) {
		dpri("i%d: err %ld\n", bindex, PTR_ERR(inode));
		return -1;
	}

	/* the type of i_blocks depends upon CONFIG_LBDAF */
	BUILD_BUG_ON(sizeof(inode->i_blocks) != sizeof(unsigned long)
		     && sizeof(inode->i_blocks) != sizeof(u64));

	dpri("i%d: %p, i%lu, %s, cnt %d, nl %u, 0%o, sz %llu, blk %llu,"
	     " ct %lld, np %lu, st 0x%lx, f 0x%x, v %llu, g %x\n",
	     bindex, inode,
	     inode->i_ino, inode->i_sb ? au_sbtype(inode->i_sb) : "??",
	     atomic_read(&inode->i_count), inode->i_nlink, inode->i_mode,
	     i_size_read(inode), (unsigned long long)inode->i_blocks,
	     (long long)timespec_to_ns(&inode->i_ctime) & 0x0ffff,
	     inode->i_mapping ? inode->i_mapping->nrpages : 0,
	     inode->i_state, inode->i_flags, inode->i_version,
	     inode->i_generation);
	return 0;
}

void au_dpri_inode(struct inode *inode)
{
	struct au_iinfo *iinfo;
	aufs_bindex_t bindex;
	int err;

	err = do_pri_inode(-1, inode);
	if (err || !au_test_aufs(inode->i_sb))
		return;

	iinfo = au_ii(inode);
	if (!iinfo)
		return;
	dpri("i-1: bstart %d, bend %d\n",
	     iinfo->ii_bstart, iinfo->ii_bend);
	if (iinfo->ii_bstart < 0)
		return;
	for (bindex = iinfo->ii_bstart; bindex <= iinfo->ii_bend; bindex++)
		do_pri_inode(bindex, iinfo->ii_hinode[0 + bindex].hi_inode);
}
