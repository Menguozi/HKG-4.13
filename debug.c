/*
 * f2fs debugging statistics
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 * Copyright (c) 2012 Linux Foundation
 * Copyright (c) 2012 Greg Kroah-Hartman <gregkh@linuxfoundation.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fs.h>
#include <linux/backing-dev.h>
#include <linux/f2fs_fs.h>
#include <linux/blkdev.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "f2fs.h"
#include "node.h"
#include "segment.h"
#include "gc.h"

static LIST_HEAD(f2fs_stat_list);
static struct dentry *f2fs_debugfs_root;
static DEFINE_MUTEX(f2fs_stat_mutex);

static void update_general_status(struct f2fs_sb_info *sbi)
{
	struct f2fs_stat_info *si = F2FS_STAT(sbi);
	int i;

	/* validation check of the segment numbers */
	si->hit_largest = atomic64_read(&sbi->read_hit_largest);
	si->hit_cached = atomic64_read(&sbi->read_hit_cached);
	si->hit_rbtree = atomic64_read(&sbi->read_hit_rbtree);
	si->hit_total = si->hit_largest + si->hit_cached + si->hit_rbtree;
	si->total_ext = atomic64_read(&sbi->total_hit_ext);
	si->ext_tree = atomic_read(&sbi->total_ext_tree);
	si->zombie_tree = atomic_read(&sbi->total_zombie_tree);
	si->ext_node = atomic_read(&sbi->total_ext_node);
	si->ndirty_node = get_pages(sbi, F2FS_DIRTY_NODES);
	si->ndirty_dent = get_pages(sbi, F2FS_DIRTY_DENTS);
	si->ndirty_meta = get_pages(sbi, F2FS_DIRTY_META);
	si->ndirty_data = get_pages(sbi, F2FS_DIRTY_DATA);
	si->ndirty_imeta = get_pages(sbi, F2FS_DIRTY_IMETA);
	si->ndirty_dirs = sbi->ndirty_inode[DIR_INODE];
	si->ndirty_files = sbi->ndirty_inode[FILE_INODE];
	si->ndirty_all = sbi->ndirty_inode[DIRTY_META];
	si->inmem_pages = get_pages(sbi, F2FS_INMEM_PAGES);
	si->aw_cnt = atomic_read(&sbi->aw_cnt);
	si->vw_cnt = atomic_read(&sbi->vw_cnt);
	si->max_aw_cnt = atomic_read(&sbi->max_aw_cnt);
	si->max_vw_cnt = atomic_read(&sbi->max_vw_cnt);
	si->nr_wb_cp_data = get_pages(sbi, F2FS_WB_CP_DATA);
	si->nr_wb_data = get_pages(sbi, F2FS_WB_DATA);
	if (SM_I(sbi) && SM_I(sbi)->fcc_info) {
		si->nr_flushed =
			atomic_read(&SM_I(sbi)->fcc_info->issued_flush);
		si->nr_flushing =
			atomic_read(&SM_I(sbi)->fcc_info->issing_flush);
	}
	if (SM_I(sbi) && SM_I(sbi)->dcc_info) {
		si->nr_discarded =
			atomic_read(&SM_I(sbi)->dcc_info->issued_discard);
		si->nr_discarding =
			atomic_read(&SM_I(sbi)->dcc_info->issing_discard);
		si->nr_discard_cmd =
			atomic_read(&SM_I(sbi)->dcc_info->discard_cmd_cnt);
		si->undiscard_blks = SM_I(sbi)->dcc_info->undiscard_blks;
	}
	si->total_count = (int)sbi->user_block_count / sbi->blocks_per_seg;
	si->rsvd_segs = reserved_segments(sbi);
	si->overp_segs = overprovision_segments(sbi);
	si->valid_count = valid_user_blocks(sbi);
	si->discard_blks = discard_blocks(sbi);
	si->valid_node_count = valid_node_count(sbi);
	si->valid_inode_count = valid_inode_count(sbi);
	si->inline_xattr = atomic_read(&sbi->inline_xattr);
	si->inline_inode = atomic_read(&sbi->inline_inode);
	si->inline_dir = atomic_read(&sbi->inline_dir);
	si->append = sbi->im[APPEND_INO].ino_num;
	si->update = sbi->im[UPDATE_INO].ino_num;
	si->orphans = sbi->im[ORPHAN_INO].ino_num;
	si->utilization = utilization(sbi);

	si->free_segs = free_segments(sbi);
	si->free_secs = free_sections(sbi);
	si->prefree_count = prefree_segments(sbi);
	si->dirty_count = dirty_segments(sbi);
	si->node_pages = NODE_MAPPING(sbi)->nrpages;
	si->meta_pages = META_MAPPING(sbi)->nrpages;
	si->nats = NM_I(sbi)->nat_cnt;
	si->dirty_nats = NM_I(sbi)->dirty_nat_cnt;
	si->sits = MAIN_SEGS(sbi);
	si->dirty_sits = SIT_I(sbi)->dirty_sentries;
	si->free_nids = NM_I(sbi)->nid_cnt[FREE_NID_LIST];
	si->avail_nids = NM_I(sbi)->available_nids;
	si->alloc_nids = NM_I(sbi)->nid_cnt[ALLOC_NID_LIST];
	si->bg_gc = sbi->bg_gc;
	si->util_free = (int)(free_user_blocks(sbi) >> sbi->log_blocks_per_seg)
		* 100 / (int)(sbi->user_block_count >> sbi->log_blocks_per_seg)
		/ 2;
	si->util_valid = (int)(written_block_count(sbi) >>
						sbi->log_blocks_per_seg)
		* 100 / (int)(sbi->user_block_count >> sbi->log_blocks_per_seg)
		/ 2;
	si->util_invalid = 50 - si->util_free - si->util_valid;
	for (i = CURSEG_HOT_DATA; i <= CURSEG_COLD_NODE; i++) {
		struct curseg_info *curseg = CURSEG_I(sbi, i);
		si->curseg[i] = curseg->segno;
		si->cursec[i] = GET_SEC_FROM_SEG(sbi, curseg->segno);
		si->curzone[i] = GET_ZONE_FROM_SEC(sbi, si->cursec[i]);
	}

	for (i = 0; i < 2; i++) {
		si->segment_count[i] = sbi->segment_count[i];
		si->block_count[i] = sbi->block_count[i];
	}

	si->inplace_count = atomic_read(&sbi->inplace_count);
}

/*
 * This function calculates BDF of every segments
 */
static void update_sit_info(struct f2fs_sb_info *sbi)
{
	struct f2fs_stat_info *si = F2FS_STAT(sbi);
	unsigned long long blks_per_sec, hblks_per_sec, total_vblocks;
	unsigned long long bimodal, dist;
	unsigned int segno, vblocks;
	int ndirty = 0;

	bimodal = 0;
	total_vblocks = 0;
	blks_per_sec = BLKS_PER_SEC(sbi);
	hblks_per_sec = blks_per_sec / 2;
	for (segno = 0; segno < MAIN_SEGS(sbi); segno += sbi->segs_per_sec) {
		vblocks = get_valid_blocks(sbi, segno, true);
		dist = abs(vblocks - hblks_per_sec);
		bimodal += dist * dist;

		if (vblocks > 0 && vblocks < blks_per_sec) {
			total_vblocks += vblocks;
			ndirty++;
		}
	}
	dist = div_u64(MAIN_SECS(sbi) * hblks_per_sec * hblks_per_sec, 100);
	si->bimodal = div64_u64(bimodal, dist);
	if (si->dirty_count)
		si->avg_vblocks = div_u64(total_vblocks, ndirty);
	else
		si->avg_vblocks = 0;
}

/*
 * This function calculates memory footprint.
 */
static void update_mem_info(struct f2fs_sb_info *sbi)
{
	struct f2fs_stat_info *si = F2FS_STAT(sbi);
	unsigned npages;
	int i;

	if (si->base_mem)
		goto get_cache;

	/* build stat */
	si->base_mem = sizeof(struct f2fs_stat_info);

	/* build superblock */
	si->base_mem += sizeof(struct f2fs_sb_info) + sbi->sb->s_blocksize;
	si->base_mem += 2 * sizeof(struct f2fs_inode_info);
	si->base_mem += sizeof(*sbi->ckpt);
	si->base_mem += sizeof(struct percpu_counter) * NR_COUNT_TYPE;

	/* build sm */
	si->base_mem += sizeof(struct f2fs_sm_info);

	/* build sit */
	si->base_mem += sizeof(struct sit_info);
	si->base_mem += MAIN_SEGS(sbi) * sizeof(struct seg_entry);
	si->base_mem += f2fs_bitmap_size(MAIN_SEGS(sbi));
	si->base_mem += 2 * SIT_VBLOCK_MAP_SIZE * MAIN_SEGS(sbi);
	if (f2fs_discard_en(sbi))
		si->base_mem += SIT_VBLOCK_MAP_SIZE * MAIN_SEGS(sbi);
	si->base_mem += SIT_VBLOCK_MAP_SIZE;
	if (sbi->segs_per_sec > 1)
		si->base_mem += MAIN_SECS(sbi) * sizeof(struct sec_entry);
	si->base_mem += __bitmap_size(sbi, SIT_BITMAP);

	/* build free segmap */
	si->base_mem += sizeof(struct free_segmap_info);
	si->base_mem += f2fs_bitmap_size(MAIN_SEGS(sbi));
	si->base_mem += f2fs_bitmap_size(MAIN_SECS(sbi));

	/* build curseg */
	si->base_mem += sizeof(struct curseg_info) * NR_CURSEG_TYPE;
	si->base_mem += PAGE_SIZE * NR_CURSEG_TYPE;

	/* build dirty segmap */
	si->base_mem += sizeof(struct dirty_seglist_info);
	si->base_mem += NR_DIRTY_TYPE * f2fs_bitmap_size(MAIN_SEGS(sbi));
	si->base_mem += f2fs_bitmap_size(MAIN_SECS(sbi));

	/* build nm */
	si->base_mem += sizeof(struct f2fs_nm_info);
	si->base_mem += __bitmap_size(sbi, NAT_BITMAP);
	si->base_mem += (NM_I(sbi)->nat_bits_blocks << F2FS_BLKSIZE_BITS);
	si->base_mem += NM_I(sbi)->nat_blocks * NAT_ENTRY_BITMAP_SIZE;
	si->base_mem += NM_I(sbi)->nat_blocks / 8;
	si->base_mem += NM_I(sbi)->nat_blocks * sizeof(unsigned short);

get_cache:
	si->cache_mem = 0;

	/* build gc */
	if (sbi->gc_thread)
		si->cache_mem += sizeof(struct f2fs_gc_kthread);

	/* build merge flush thread */
	if (SM_I(sbi)->fcc_info)
		si->cache_mem += sizeof(struct flush_cmd_control);
	if (SM_I(sbi)->dcc_info) {
		si->cache_mem += sizeof(struct discard_cmd_control);
		si->cache_mem += sizeof(struct discard_cmd) *
			atomic_read(&SM_I(sbi)->dcc_info->discard_cmd_cnt);
	}

	/* free nids */
	si->cache_mem += (NM_I(sbi)->nid_cnt[FREE_NID_LIST] +
				NM_I(sbi)->nid_cnt[ALLOC_NID_LIST]) *
				sizeof(struct free_nid);
	si->cache_mem += NM_I(sbi)->nat_cnt * sizeof(struct nat_entry);
	si->cache_mem += NM_I(sbi)->dirty_nat_cnt *
					sizeof(struct nat_entry_set);
	si->cache_mem += si->inmem_pages * sizeof(struct inmem_pages);
	for (i = 0; i <= ORPHAN_INO; i++)
		si->cache_mem += sbi->im[i].ino_num * sizeof(struct ino_entry);
	si->cache_mem += atomic_read(&sbi->total_ext_tree) *
						sizeof(struct extent_tree);
	si->cache_mem += atomic_read(&sbi->total_ext_node) *
						sizeof(struct extent_node);

	si->page_mem = 0;
	npages = NODE_MAPPING(sbi)->nrpages;
	si->page_mem += (unsigned long long)npages << PAGE_SHIFT;
	npages = META_MAPPING(sbi)->nrpages;
	si->page_mem += (unsigned long long)npages << PAGE_SHIFT;
}

static int stat_show(struct seq_file *s, void *v)
{
	struct f2fs_stat_info *si;
	int i = 0;
	int j;

	unsigned int unzero_updated = 0;
	unsigned int unzero_read = 0;
	unsigned long total_blocks = 0;

	mutex_lock(&f2fs_stat_mutex);
	list_for_each_entry(si, &f2fs_stat_list, stat_list) {
		update_general_status(si->sbi);

		seq_printf(s, "\n=====[ partition info(%pg). #%d, %s]=====\n",
			si->sbi->sb->s_bdev, i++,
			f2fs_readonly(si->sbi->sb) ? "RO": "RW");
		seq_printf(s, "[SB: 1] [CP: 2] [SIT: %d] [NAT: %d] ",
			   si->sit_area_segs, si->nat_area_segs);
		seq_printf(s, "[SSA: %d] [MAIN: %d",
			   si->ssa_area_segs, si->main_area_segs);
		seq_printf(s, "(OverProv:%d Resv:%d)]\n\n",
			   si->overp_segs, si->rsvd_segs);
		if (test_opt(si->sbi, DISCARD))
			seq_printf(s, "Utilization: %u%% (%u valid blocks, %u discard blocks)\n",
				si->utilization, si->valid_count, si->discard_blks);
		else
			seq_printf(s, "Utilization: %u%% (%u valid blocks)\n",
				si->utilization, si->valid_count);

		seq_printf(s, "  - Node: %u (Inode: %u, ",
			   si->valid_node_count, si->valid_inode_count);
		seq_printf(s, "Other: %u)\n  - Data: %u\n",
			   si->valid_node_count - si->valid_inode_count,
			   si->valid_count - si->valid_node_count);
		seq_printf(s, "  - Inline_xattr Inode: %u\n",
			   si->inline_xattr);
		seq_printf(s, "  - Inline_data Inode: %u\n",
			   si->inline_inode);
		seq_printf(s, "  - Inline_dentry Inode: %u\n",
			   si->inline_dir);
		seq_printf(s, "  - Orphan/Append/Update Inode: %u, %u, %u\n",
			   si->orphans, si->append, si->update);
		seq_printf(s, "\nMain area: %d segs, %d secs %d zones\n",
			   si->main_area_segs, si->main_area_sections,
			   si->main_area_zones);
		seq_printf(s, "  - COLD  data: %d, %d, %d\n",
			   si->curseg[CURSEG_COLD_DATA],
			   si->cursec[CURSEG_COLD_DATA],
			   si->curzone[CURSEG_COLD_DATA]);
		seq_printf(s, "  - WARM  data: %d, %d, %d\n",
			   si->curseg[CURSEG_WARM_DATA],
			   si->cursec[CURSEG_WARM_DATA],
			   si->curzone[CURSEG_WARM_DATA]);
		seq_printf(s, "  - HOT   data: %d, %d, %d\n",
			   si->curseg[CURSEG_HOT_DATA],
			   si->cursec[CURSEG_HOT_DATA],
			   si->curzone[CURSEG_HOT_DATA]);
		seq_printf(s, "  - Dir   dnode: %d, %d, %d\n",
			   si->curseg[CURSEG_HOT_NODE],
			   si->cursec[CURSEG_HOT_NODE],
			   si->curzone[CURSEG_HOT_NODE]);
		seq_printf(s, "  - File   dnode: %d, %d, %d\n",
			   si->curseg[CURSEG_WARM_NODE],
			   si->cursec[CURSEG_WARM_NODE],
			   si->curzone[CURSEG_WARM_NODE]);
		seq_printf(s, "  - Indir nodes: %d, %d, %d\n",
			   si->curseg[CURSEG_COLD_NODE],
			   si->cursec[CURSEG_COLD_NODE],
			   si->curzone[CURSEG_COLD_NODE]);
		seq_printf(s, "\n  - Valid: %d\n  - Dirty: %d\n",
			   si->main_area_segs - si->dirty_count -
			   si->prefree_count - si->free_segs,
			   si->dirty_count);
		seq_printf(s, "  - Prefree: %d\n  - Free: %d (%d)\n\n",
			   si->prefree_count, si->free_segs, si->free_secs);
		seq_printf(s, "CP calls: %d (BG: %d)\n",
				si->cp_count, si->bg_cp_count);
		seq_printf(s, "GC calls: %d (BG: %d)\n",
			   si->call_count, si->bg_gc);
		seq_printf(s, "  - data segments : %d (%d)\n",
				si->data_segs, si->bg_data_segs);
		seq_printf(s, "  - node segments : %d (%d)\n",
				si->node_segs, si->bg_node_segs);
		seq_printf(s, "Try to move %d blocks (BG: %d)\n", si->tot_blks,
				si->bg_data_blks + si->bg_node_blks);
		seq_printf(s, "  - data blocks : %d (%d)\n", si->data_blks,
				si->bg_data_blks);
		seq_printf(s, "  - node blocks : %d (%d)\n", si->node_blks,
				si->bg_node_blks);
		seq_puts(s, "\nExtent Cache:\n");
		seq_printf(s, "  - Hit Count: L1-1:%llu L1-2:%llu L2:%llu\n",
				si->hit_largest, si->hit_cached,
				si->hit_rbtree);
		seq_printf(s, "  - Hit Ratio: %llu%% (%llu / %llu)\n",
				!si->total_ext ? 0 :
				div64_u64(si->hit_total * 100, si->total_ext),
				si->hit_total, si->total_ext);
		seq_printf(s, "  - Inner Struct Count: tree: %d(%d), node: %d\n",
				si->ext_tree, si->zombie_tree, si->ext_node);
		seq_puts(s, "\nBalancing F2FS Async:\n");
		seq_printf(s, "  - IO (CP: %4d, Data: %4d, Flush: (%4d %4d), "
			"Discard: (%4d %4d)) cmd: %4d undiscard:%4u\n",
			   si->nr_wb_cp_data, si->nr_wb_data,
			   si->nr_flushing, si->nr_flushed,
			   si->nr_discarding, si->nr_discarded,
			   si->nr_discard_cmd, si->undiscard_blks);
		seq_printf(s, "  - inmem: %4d, atomic IO: %4d (Max. %4d), "
			"volatile IO: %4d (Max. %4d)\n",
			   si->inmem_pages, si->aw_cnt, si->max_aw_cnt,
			   si->vw_cnt, si->max_vw_cnt);
		seq_printf(s, "  - nodes: %4d in %4d\n",
			   si->ndirty_node, si->node_pages);
		seq_printf(s, "  - dents: %4d in dirs:%4d (%4d)\n",
			   si->ndirty_dent, si->ndirty_dirs, si->ndirty_all);
		seq_printf(s, "  - datas: %4d in files:%4d\n",
			   si->ndirty_data, si->ndirty_files);
		seq_printf(s, "  - meta: %4d in %4d\n",
			   si->ndirty_meta, si->meta_pages);
		seq_printf(s, "  - imeta: %4d\n",
			   si->ndirty_imeta);
		seq_printf(s, "  - NATs: %9d/%9d\n  - SITs: %9d/%9d\n",
			   si->dirty_nats, si->nats, si->dirty_sits, si->sits);
		seq_printf(s, "  - free_nids: %9d/%9d\n  - alloc_nids: %9d\n",
			   si->free_nids, si->avail_nids, si->alloc_nids);
		seq_puts(s, "\nDistribution of User Blocks:");
		seq_puts(s, " [ valid | invalid | free ]\n");
		seq_puts(s, "  [");

		for (j = 0; j < si->util_valid; j++)
			seq_putc(s, '-');
		seq_putc(s, '|');

		for (j = 0; j < si->util_invalid; j++)
			seq_putc(s, '-');
		seq_putc(s, '|');

		for (j = 0; j < si->util_free; j++)
			seq_putc(s, '-');
		seq_puts(s, "]\n\n");
		seq_printf(s, "IPU: %u blocks\n", si->inplace_count);
		seq_printf(s, "SSR: %u blocks in %u segments\n",
			   si->block_count[SSR], si->segment_count[SSR]);
		seq_printf(s, "LFS: %u blocks in %u segments\n",
			   si->block_count[LFS], si->segment_count[LFS]);

		/* segment usage info */
		update_sit_info(si->sbi);
		seq_printf(s, "\nBDF: %u, avg. vblocks: %u\n",
			   si->bimodal, si->avg_vblocks);

		/* memory footprint */
		update_mem_info(si->sbi);
		seq_printf(s, "\nMemory: %llu KB\n",
			(si->base_mem + si->cache_mem + si->page_mem) >> 10);
		seq_printf(s, "  - static: %llu KB\n",
				si->base_mem >> 10);
		seq_printf(s, "  - cached: %llu KB\n",
				si->cache_mem >> 10);
		seq_printf(s, "  - paged : %llu KB\n",
				si->page_mem >> 10);
<<<<<<< HEAD
//add finalG
		/*
		for(i = 0; i < MAIN_SEGS(si->sbi) * si->sbi->blocks_per_seg; i++){
			if((si->sbi->blk_cnt_en + i)->IRR != MAX_IRR)
				seq_printf(s, "IRR: %u, LWS: %u\n", (si->sbi->blk_cnt_en + i)->IRR, (si->sbi->blk_cnt_en + i)->LWS);
		}
		
		//?????????????????????????????????
		struct hotness_curseg_info *array = SM_I(si->sbi)->hotness_curseg_array;
		for (i = 0; i < NR_HOTNESS_CURSEG_DATA_TYPE; i++) {
			seq_printf(s, "hotness curseg %d =  %u; ", i, array[i].segno);
		}
		seq_printf(s, "\nthe nat blocks = %u\n",si->sbi->nm_info->nat_blocks);
		*/
		//??????????????????
		//seq_printf(s, "now time: %llu\n", get_mtime(si->sbi));
		
		//?????????????????????
		//????????????????????????????????????????????????
		//seq_printf(s, "\nthe utilization is %d, it is %d time to sample\n",si->sbi->util, si->sbi->sample_times);
		//????????????????????????irr???lws
		//seq_printf(s, "sample result:\n");
		//for(i = 0; i < 90000; i++){
		//	seq_printf(s, "irr: %u, lws: %u\n", *(si->sbi->sample_irr_array + i), *(si->sbi->sample_lws_array + i));
		//}
		
		/**
		//???????????????block???lws???????????????cws?????????????????????????????????????????????
		//????????????cnt???????????????valid map???valid???????????????
		int valid_cnt = 0;
		int valid_cnt1 = 0;
		struct seg_entry *sentry;
		struct sit_info *sit_i = SIT_I(si->sbi);
		mutex_lock(&sit_i->sentry_lock);

		for(i = 0; i < MAIN_SEGS(si->sbi); i++){
			
			sentry = get_seg_entry(si->sbi, i);
			valid_cnt += sentry->valid_blocks;
			for(j =0; j < 512; j++){
				if(f2fs_test_bit(j, sentry->cur_valid_map)){
					valid_cnt1++;
					seq_printf(s, "%d: %u\n", i * 512 + j, si->sbi->blk_cnt_en[i * 512 + j].LWS); 
				}
			}
			
		}
		mutex_unlock(&sit_i->sentry_lock);
		seq_printf(s, "valid cnt = %d\n", valid_cnt); 
		seq_printf(s, "valid cnt1 = %d\n", valid_cnt1); 
		*/

		
		/*
		//???????????????seg???valid???level???????????????????????????valid???
		struct seg_entry *sentry;
		struct sit_info *sit_i = SIT_I(si->sbi);
		int level_nr = 5;
		int level_cnt[20];
		int k = 0, cur_level = 0;
		mutex_lock(&sit_i->sentry_lock);

		for(i = 0; i < MAIN_SEGS(si->sbi); i++){
			//??????level???cnt
			for(k = 0; k < level_nr; k++){
				level_cnt[k] = 0;
			}
			int valid_cnt = 0, level_width = si->sbi->block_count[WARM_DATA_LFS] / level_nr;
			sentry = get_seg_entry(si->sbi, i);
			valid_cnt = sentry->valid_blocks;
			for(j = 0; j < 512; j++){
				if(f2fs_test_bit(j, sentry->cur_valid_map)){
					cur_level = si->sbi->blk_cnt_en[i * 512 + j].LWS / level_width;
					if(cur_level == level_nr)
						cur_level = level_nr - 1; 
					level_cnt[cur_level]++;
				}
			}
			seq_printf(s, "%d: %d, %d, %d, %d, %d, \n", valid_cnt, level_cnt[0], level_cnt[1], level_cnt[2], level_cnt[3], level_cnt[4]);
		}
		mutex_unlock(&sit_i->sentry_lock);
		*/
//add finalG

		seq_printf(s, "format: \"updated : read : FUD: LRUD\" \n")	;
		total_blocks = MAIN_SEGS(si->sbi) * si->sbi->blocks_per_seg;
		j = 0;
		for ( i = 0; i < total_blocks; i++)
		{
			if((si->sbi->blk_cnt_en + i)->updated != 0)
				++unzero_updated;
			if((si->sbi->blk_cnt_en + i)->read != 0)
				++unzero_read;
			// if(i%32 == 0)
			// 	seq_printf(s, "\n");
			if ((si->sbi->blk_cnt_en + i)->updated || (si->sbi->blk_cnt_en + i)->read 
			|| (si->sbi->blk_cnt_en + i)->lastlast || (si->sbi->blk_cnt_en + i)->last){
				seq_printf(s, "%u\t%u\t%u\t%u\n", (si->sbi->blk_cnt_en + i)->updated,
					(si->sbi->blk_cnt_en + i)->read,
					(si->sbi->blk_cnt_en + i)->lastlast ? (si->sbi->blk_cnt_en + i)->last - (si->sbi->blk_cnt_en + i)->lastlast : 0,
					(si->sbi->blk_cnt_en + i)->last ? si->sbi->updated_pages - (si->sbi->blk_cnt_en + i)->last : 0);
				j++;
			}
		}
		seq_printf(s, "%u\t%u\t%u\t%u\n", 0, 0, 0, 0);
		seq_printf(s, "%u\n", j);
		// seq_printf(s, "\n updated blocks:  %u \n", unzero_updated);
		// seq_printf(s, " read blocks:  %u \n", unzero_read);
=======
>>>>>>> parent of 5f6b651 (The M2H implementation of F2FS on linux-4.13.0)
	}
	mutex_unlock(&f2fs_stat_mutex);
	return 0;
}

static int stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, stat_show, inode->i_private);
}

static const struct file_operations stat_fops = {
	.owner = THIS_MODULE,
	.open = stat_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int f2fs_build_stats(struct f2fs_sb_info *sbi)
{
	struct f2fs_super_block *raw_super = F2FS_RAW_SUPER(sbi);
	struct f2fs_stat_info *si;

	si = kzalloc(sizeof(struct f2fs_stat_info), GFP_KERNEL);
	if (!si)
		return -ENOMEM;

	si->all_area_segs = le32_to_cpu(raw_super->segment_count);
	si->sit_area_segs = le32_to_cpu(raw_super->segment_count_sit);
	si->nat_area_segs = le32_to_cpu(raw_super->segment_count_nat);
	si->ssa_area_segs = le32_to_cpu(raw_super->segment_count_ssa);
	si->main_area_segs = le32_to_cpu(raw_super->segment_count_main);
	si->main_area_sections = le32_to_cpu(raw_super->section_count);
	si->main_area_zones = si->main_area_sections /
				le32_to_cpu(raw_super->secs_per_zone);
	si->sbi = sbi;
	sbi->stat_info = si;

	atomic64_set(&sbi->total_hit_ext, 0);
	atomic64_set(&sbi->read_hit_rbtree, 0);
	atomic64_set(&sbi->read_hit_largest, 0);
	atomic64_set(&sbi->read_hit_cached, 0);

	atomic_set(&sbi->inline_xattr, 0);
	atomic_set(&sbi->inline_inode, 0);
	atomic_set(&sbi->inline_dir, 0);
	atomic_set(&sbi->inplace_count, 0);

	atomic_set(&sbi->aw_cnt, 0);
	atomic_set(&sbi->vw_cnt, 0);
	atomic_set(&sbi->max_aw_cnt, 0);
	atomic_set(&sbi->max_vw_cnt, 0);

	mutex_lock(&f2fs_stat_mutex);
	list_add_tail(&si->stat_list, &f2fs_stat_list);
	mutex_unlock(&f2fs_stat_mutex);

	return 0;
}

void f2fs_destroy_stats(struct f2fs_sb_info *sbi)
{
	struct f2fs_stat_info *si = F2FS_STAT(sbi);

	mutex_lock(&f2fs_stat_mutex);
	list_del(&si->stat_list);
	mutex_unlock(&f2fs_stat_mutex);

	kfree(si);
}

int __init f2fs_create_root_stats(void)
{
	struct dentry *file;

	f2fs_debugfs_root = debugfs_create_dir("f2fs", NULL);
	if (!f2fs_debugfs_root)
		return -ENOMEM;

	file = debugfs_create_file("status", S_IRUGO, f2fs_debugfs_root,
			NULL, &stat_fops);
	if (!file) {
		debugfs_remove(f2fs_debugfs_root);
		f2fs_debugfs_root = NULL;
		return -ENOMEM;
	}

	return 0;
}

void f2fs_destroy_root_stats(void)
{
	if (!f2fs_debugfs_root)
		return;

	debugfs_remove_recursive(f2fs_debugfs_root);
	f2fs_debugfs_root = NULL;
}
