// my_predictor.h
// This file contains a sample gshare_predictor class.
// It is a simple 32,768-entry gshare with a history length of 15.

static void lru_touch (unsigned int *order, unsigned int ways, unsigned int way) {
	unsigned int pos = 0;
	while (order[pos] != way) pos++;
	for (unsigned int i = pos; i > 0; i--) order[i] = order[i-1];
	order[0] = way;
}

static unsigned int lru_pick_way (bool *valid, unsigned int *order, unsigned int ways, bool hit, unsigned int hit_way) {
	if (hit) return hit_way;
	for (unsigned int w = 0; w < ways; w++)
		if (!valid[w]) return w;
	return order[ways - 1];
}

class gshare_update : public branch_update {
public:
	unsigned int index;
};

class gshare_predictor : public branch_predictor {
public:
#define HISTORY_LENGTH	15
#define TABLE_BITS	15
	gshare_update u;
	branch_info bi;
	unsigned int history;
	unsigned char tab[1<<TABLE_BITS];

	gshare_predictor (void) : history(0) { 
		memset (tab, 0, sizeof (tab));
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = 
				  (history << (TABLE_BITS - HISTORY_LENGTH)) 
				^ (b.address & ((1<<TABLE_BITS)-1));
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);
		}
		u.target_prediction (0);
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {
			unsigned char *c = &tab[((gshare_update*)u)->index];
			if (taken) {
				if (*c < 3) (*c)++;
			} else {
				if (*c > 0) (*c)--;
			}
			history <<= 1;
			history |= taken;
			history &= (1<<HISTORY_LENGTH)-1;
		}
	}
};

class pm_update : public branch_update {
public:
        bool global_hit;
        unsigned int global_index;
        unsigned int global_way;
        unsigned int global_tag;
        unsigned int bimodal_index;
};

class pm_predictor : public branch_predictor {
public:
#define PM_GHR_BITS		15
#define PM_GLOBAL_INDEX_BITS	9
#define PM_GLOBAL_TAG_BITS	6
#define PM_GLOBAL_WAYS		4
#define PM_GLOBAL_SETS		(1 << PM_GLOBAL_INDEX_BITS)
#define PM_BIMODAL_BITS		12
#define PM_BIMODAL_SIZE		(1 << PM_BIMODAL_BITS)

        pm_update u;
        branch_info bi;
        unsigned int ghr;
        unsigned char bimodal[PM_BIMODAL_SIZE];
        bool global_valid[PM_GLOBAL_SETS][PM_GLOBAL_WAYS];
        unsigned int global_tag[PM_GLOBAL_SETS][PM_GLOBAL_WAYS];
        unsigned char global_ctr[PM_GLOBAL_SETS][PM_GLOBAL_WAYS];
        unsigned int global_order[PM_GLOBAL_SETS][PM_GLOBAL_WAYS];

        pm_predictor (void) : ghr(0) {
                memset (bimodal, 0, sizeof (bimodal));
                memset (global_valid, 0, sizeof (global_valid));
                memset (global_tag, 0, sizeof (global_tag));
                memset (global_ctr, 0, sizeof (global_ctr));
                for (unsigned int s = 0; s < PM_GLOBAL_SETS; s++)
                        for (unsigned int w = 0; w < PM_GLOBAL_WAYS; w++)
                                global_order[s][w] = w;
        }

        branch_update *predict (branch_info & b) {
                bi = b;
                if (b.br_flags & BR_CONDITIONAL) {
                        unsigned int hash = (b.address & ((1 << PM_GHR_BITS) - 1)) ^ ghr;
                        unsigned int index = (hash >> PM_GLOBAL_TAG_BITS) & (PM_GLOBAL_SETS - 1);
                        unsigned int tag = hash & ((1 << PM_GLOBAL_TAG_BITS) - 1);
                        unsigned int bindex = b.address & (PM_BIMODAL_SIZE - 1);

                        u.global_index = index;
                        u.global_tag = tag;
                        u.bimodal_index = bindex;
                        u.global_hit = false;
                        u.global_way = 0;

                        for (unsigned int w = 0; w < PM_GLOBAL_WAYS; w++) {
                                if (global_valid[index][w] && global_tag[index][w] == tag) {
                                        u.global_hit = true;
                                        u.global_way = w;
                                        break;
                                }
                        }

                        if (u.global_hit)
                                u.direction_prediction (global_ctr[index][u.global_way] >= 2);
                        else
                                u.direction_prediction (bimodal[bindex] >= 2);
                } else {
                        u.direction_prediction (true);
                }
                u.target_prediction (0);
                return &u;
        }

        void update (branch_update *bu, bool taken, unsigned int target) {
                if (!(bi.br_flags & BR_CONDITIONAL)) return;
                pm_update *pu = (pm_update *) bu;

                unsigned char &bc = bimodal[pu->bimodal_index];
                if (taken) { if (bc < 3) bc++; } else { if (bc > 0) bc--; }

                unsigned int index = pu->global_index;
                unsigned int way = lru_pick_way (global_valid[index], global_order[index], PM_GLOBAL_WAYS, pu->global_hit, pu->global_way);

                if (!pu->global_hit) {
                        global_valid[index][way] = true;
                        global_tag[index][way] = pu->global_tag;
                        global_ctr[index][way] = taken ? 2 : 1;
                } else {
                        unsigned char &gc = global_ctr[index][way];
                        if (taken) { if (gc < 3) gc++; } else { if (gc > 0) gc--; }
                }
                lru_touch (global_order[index], PM_GLOBAL_WAYS, way);

                ghr <<= 1;
                ghr |= taken;
                ghr &= (1 << PM_GHR_BITS) - 1;
        }

};

class cpm_update : public branch_update {
public:
        unsigned int btb_index;
        unsigned int btb_tag;
        unsigned int btb_offset;
        unsigned int btb_way;
        bool btb_hit;
        unsigned int ibtb_index;
        unsigned int ibtb_tag;
        unsigned int ibtb_way;
        bool ibtb_hit;
        bool is_indirect;
};

class cpm_predictor : public branch_predictor {
public:
#define CPM_BTB_INDEX_BITS	9
#define CPM_BTB_TAG_BITS	9
#define CPM_BTB_WAYS		4
#define CPM_BTB_SETS		(1 << CPM_BTB_INDEX_BITS)
#define CPM_IBTB_HASH_BITS	15
#define CPM_IBTB_INDEX_BITS	8
#define CPM_IBTB_WAYS		4
#define CPM_IBTB_SETS		(1 << CPM_IBTB_INDEX_BITS)

        pm_predictor direction;
        cpm_update u;

        bool btb_valid[CPM_BTB_SETS][CPM_BTB_WAYS];
        unsigned int btb_tag[CPM_BTB_SETS][CPM_BTB_WAYS];
        unsigned int btb_offset[CPM_BTB_SETS][CPM_BTB_WAYS];
        bool btb_has_taken[CPM_BTB_SETS][CPM_BTB_WAYS];
        unsigned int btb_target_taken[CPM_BTB_SETS][CPM_BTB_WAYS];
        bool btb_has_nottaken[CPM_BTB_SETS][CPM_BTB_WAYS];
        unsigned int btb_target_nottaken[CPM_BTB_SETS][CPM_BTB_WAYS];
        unsigned int btb_order[CPM_BTB_SETS][CPM_BTB_WAYS];

        bool ibtb_valid[CPM_IBTB_SETS][CPM_IBTB_WAYS];
        unsigned int ibtb_tag[CPM_IBTB_SETS][CPM_IBTB_WAYS];
        unsigned int ibtb_target[CPM_IBTB_SETS][CPM_IBTB_WAYS];
        unsigned int ibtb_order[CPM_IBTB_SETS][CPM_IBTB_WAYS];

        cpm_predictor (void) {
                memset (btb_valid, 0, sizeof (btb_valid));
                memset (btb_tag, 0, sizeof (btb_tag));
                memset (btb_offset, 0, sizeof (btb_offset));
                memset (btb_has_taken, 0, sizeof (btb_has_taken));
                memset (btb_target_taken, 0, sizeof (btb_target_taken));
                memset (btb_has_nottaken, 0, sizeof (btb_has_nottaken));
                memset (btb_target_nottaken, 0, sizeof (btb_target_nottaken));
                memset (ibtb_valid, 0, sizeof (ibtb_valid));
                memset (ibtb_tag, 0, sizeof (ibtb_tag));
                memset (ibtb_target, 0, sizeof (ibtb_target));
                for (unsigned int s = 0; s < CPM_BTB_SETS; s++)
                        for (unsigned int w = 0; w < CPM_BTB_WAYS; w++)
                                btb_order[s][w] = w;
                for (unsigned int s = 0; s < CPM_IBTB_SETS; s++)
                        for (unsigned int w = 0; w < CPM_IBTB_WAYS; w++)
                                ibtb_order[s][w] = w;
        }

        unsigned int btb_lookup (unsigned int index, unsigned int tag, unsigned int offset, bool &hit) {
                hit = false;
                for (unsigned int w = 0; w < CPM_BTB_WAYS; w++) {
                        if (btb_valid[index][w] && btb_tag[index][w] == tag && btb_offset[index][w] == offset) {
                                hit = true;
                                return w;
                        }
                }
                return 0;
        }

        unsigned int ibtb_lookup (unsigned int index, unsigned int tag, bool &hit) {
                hit = false;
                for (unsigned int w = 0; w < CPM_IBTB_WAYS; w++) {
                        if (ibtb_valid[index][w] && ibtb_tag[index][w] == tag) {
                                hit = true;
                                return w;
                        }
                }
                return 0;
        }

        branch_update *predict (branch_info & b) {
                branch_update *du = direction.predict (b);
                u.is_indirect = b.br_flags & BR_INDIRECT;
                u.direction_prediction ((b.br_flags & BR_CONDITIONAL) ? du->direction_prediction () : true);

                u.btb_index = (b.address >> 4) & (CPM_BTB_SETS - 1);
                u.btb_tag = (b.address >> 13) & ((1 << CPM_BTB_TAG_BITS) - 1);
                u.btb_offset = b.address & 0xF;
                u.btb_way = btb_lookup (u.btb_index, u.btb_tag, u.btb_offset, u.btb_hit);

                unsigned int target = 0;
                bool have_target = false;

                if (u.is_indirect) {
                        unsigned int hash = (b.address & ((1 << CPM_IBTB_HASH_BITS) - 1)) ^ direction.ghr;
                        u.ibtb_index = (hash >> 6) & (CPM_IBTB_SETS - 1);
                        u.ibtb_tag = (((hash >> 14) & 1) << 6) | (hash & 0x3F);
                        u.ibtb_way = ibtb_lookup (u.ibtb_index, u.ibtb_tag, u.ibtb_hit);
                        if (u.ibtb_hit) {
                                target = ibtb_target[u.ibtb_index][u.ibtb_way];
                                have_target = true;
                        }
                } else {
                        u.ibtb_hit = false;
                        u.ibtb_index = 0;
                        u.ibtb_tag = 0;
                        u.ibtb_way = 0;
                }

                if (u.direction_prediction ()) {
                        if (!have_target && u.btb_hit && btb_has_taken[u.btb_index][u.btb_way]) {
                                target = btb_target_taken[u.btb_index][u.btb_way];
                                have_target = true;
                        }
                } else {
                        if (u.btb_hit && btb_has_nottaken[u.btb_index][u.btb_way]) {
                                target = btb_target_nottaken[u.btb_index][u.btb_way];
                                have_target = true;
                        } else {
                                target = b.address + 2;
                                have_target = true;
                        }
                }

                u.target_prediction (have_target ? target : 0);
                return &u;
        }

        void update (branch_update *bu, bool taken, unsigned int target) {
                cpm_update *pu = (cpm_update *) bu;
                direction.update (&direction.u, taken, target);

                unsigned int bi = pu->btb_index;
                unsigned int bway = lru_pick_way (btb_valid[bi], btb_order[bi], CPM_BTB_WAYS, pu->btb_hit, pu->btb_way);
                btb_valid[bi][bway] = true;
                btb_tag[bi][bway] = pu->btb_tag;
                btb_offset[bi][bway] = pu->btb_offset;
                if (taken) {
                        btb_has_taken[bi][bway] = true;
                        btb_target_taken[bi][bway] = target;
                } else {
                        btb_has_nottaken[bi][bway] = true;
                        btb_target_nottaken[bi][bway] = target;
                }
                lru_touch (btb_order[bi], CPM_BTB_WAYS, bway);

                if (!taken) return;

                if (pu->is_indirect) {
                        unsigned int ii = pu->ibtb_index;
                        unsigned int iway = lru_pick_way (ibtb_valid[ii], ibtb_order[ii], CPM_IBTB_WAYS, pu->ibtb_hit, pu->ibtb_way);
                        ibtb_valid[ii][iway] = true;
                        ibtb_tag[ii][iway] = pu->ibtb_tag;
                        ibtb_target[ii][iway] = target;
                        lru_touch (ibtb_order[ii], CPM_IBTB_WAYS, iway);
                }
        }

};


