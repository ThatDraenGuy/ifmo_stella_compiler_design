use std::{
    collections::{BTreeSet, HashMap, VecDeque},
    ops::Deref,
};

use bitvector::BitVector;
use slotmap::{SecondaryMap, SlotMap};

use self::cfg::*;
use crate::{
    domain::{
        Binding, CfgBlock, CfgCallable, CfgUnit, Identifier, SsaBlock, SsaCallable, SsaUnit,
        SsaUnitProfile, Terminator,
    },
    map::{BitMap, BlockId, ViewMap},
    passes::cfg2ssa::dom::DomTree,
    CfgForm, SirPass, SsaForm,
};

pub struct Cfg2SsaPass;

impl SirPass for Cfg2SsaPass {
    type In = CfgForm;
    type Out = SsaForm;

    fn perform(input: CfgUnit) -> SsaUnit {
        SsaUnit {
            callables: input
                .callables
                .into_iter()
                .map(|callable| match callable {
                    CfgCallable::Function {
                        blocks,
                        entry,
                        locals,
                        name,
                        params,
                        result,
                    } => {
                        let ssa_blocks = convert_to_ssa(blocks, &entry, locals, &params);
                        SsaCallable::Function {
                            blocks: ssa_blocks,
                            entry,
                            name,
                            params,
                            result,
                        }
                    }
                    CfgCallable::Routine {
                        blocks,
                        entry,
                        locals,
                        name,
                        params,
                    } => {
                        let ssa_blocks = convert_to_ssa(blocks, &entry, locals, &params);
                        SsaCallable::Routine {
                            blocks: ssa_blocks,
                            entry,
                            name,
                            params,
                        }
                    }
                })
                .collect(),
            exports: input.exports,
            form: serde_json::Value::String("ssa".to_owned()),
            format: serde_json::Value::String("stella-ir".to_owned()),
            frames: input.frames,
            layouts: input.layouts,
            profile: SsaUnitProfile::try_from(String::from(input.profile)).unwrap(),
            version: serde_json::Value::String("1".to_owned()),
        }
    }
}

fn convert_to_ssa(
    blocks: Vec<CfgBlock>,
    entry: &Identifier,
    locals: Vec<Binding>,
    params: &[Binding],
) -> Vec<SsaBlock> {
    let cfg = CfgTree::new(blocks, entry);
    let entry = cfg.by_name[entry];
    let dom = DomTree::new(&cfg, entry);

    todo!()
}

fn resolve_dominators(tree: &CfgTree, entry: &Identifier) {
    let entry_key = tree.by_name[entry];

    let mut dominators = tree.blocks.bitmaps_ones();
    dominators[entry_key].clear();
    dominators[entry_key].insert(entry_key);

    let mut work = tree.blocks.work_queue(tree.reverse_postorder());
    while let Some(key) = work.pop_front() {
        let mut doms = tree.blocks.bitmap_ones();
        for pred in tree.preds[key].iter() {
            doms.intersection_inplace(&dominators[pred]);
        }
        doms.insert(key);
        if doms != dominators[key] {
            dominators[key] = doms;
            for succ in tree.succs[key].iter() {
                work.push_back(succ);
            }
        }
    }

    //ребёнок в дереве доминаторов
    let mut children = tree.blocks.bitmaps_zeros();
    let idoms = tree.blocks.view_map(|key, _| {
        let mut doms = dominators[key].clone();
        doms.remove(key);
        for dom in doms.iter() {
            let dom_doms = &dominators[key];
            //если доминаторы доминатора содержат все строгие доминаторы текущей ноды
            if doms == dom_doms.intersection(&doms) {
                children[dom].insert(key);
                return Some(dom);
            }
        }
        //нет непосредственного доминатора - только для entry-блока
        None
    });

    let mut dom_frontiers = tree.blocks.bitmaps_zeros();
    fn compute_df(
        key: BlockId,
        tree: &CfgTree,
        idoms: &ViewMap<BlockId, Option<BlockId>>,
        children: &ViewMap<BlockId, BitMap<BlockId>>,
        frontiers: &mut ViewMap<BlockId, BitMap<BlockId>>,
    ) {
        for succ in tree.succs[key].iter() {
            if matches!(idoms[succ], Some(idom) if idom != key) {
                frontiers[key].insert(succ);
            }
        }
        for child in children[key].iter() {
            compute_df(child, tree, idoms, children, frontiers);
            let mut frontier = frontiers[key].clone();
            for front in frontiers[child].iter() {
                if matches!(idoms[front], Some(idom) if idom != key) {
                    frontier.insert(front);
                }
            }
            frontiers[key] = frontier;
        }
    }
    compute_df(entry_key, tree, &idoms, &children, &mut dom_frontiers);
}

mod dom {
    use crate::{
        map::{BitMap, BlockId, ViewMap},
        passes::cfg2ssa::cfg::CfgTree,
    };

    pub struct DomTree {
        pub dominators: ViewMap<BlockId, BitMap<BlockId>>,
        pub idoms: ViewMap<BlockId, Option<BlockId>>,
        pub children: ViewMap<BlockId, BitMap<BlockId>>,
        pub frontiers: ViewMap<BlockId, BitMap<BlockId>>,
    }
    impl DomTree {
        pub fn new(cfg: &CfgTree, entry: BlockId) -> Self {
            let mut dominators = cfg.blocks.bitmaps_ones();
            dominators[entry].clear();
            dominators[entry].insert(entry);

            let mut work = cfg.blocks.work_queue(cfg.reverse_postorder());
            while let Some(key) = work.pop_front() {
                let mut doms = cfg.blocks.bitmap_ones();
                for pred in cfg.preds[key].iter() {
                    doms.intersection_inplace(&dominators[pred]);
                }
                doms.insert(key);
                if doms != dominators[key] {
                    dominators[key] = doms;
                    for succ in cfg.succs[key].iter() {
                        work.push_back(succ);
                    }
                }
            }

            //ребёнок в дереве доминаторов
            let mut children = cfg.blocks.bitmaps_zeros();
            let idoms = cfg.blocks.view_map(|key, _| {
                let mut doms = dominators[key].clone();
                doms.remove(key);
                for dom in doms.iter() {
                    let dom_doms = &dominators[key];
                    //если доминаторы доминатора содержат все строгие доминаторы текущей ноды
                    if doms == dom_doms.intersection(&doms) {
                        children[dom].insert(key);
                        return Some(dom);
                    }
                }
                //нет непосредственного доминатора - только для entry-блока
                None
            });

            let mut frontiers = cfg.blocks.bitmaps_zeros();
            fn compute_df(
                key: BlockId,
                tree: &CfgTree,
                idoms: &ViewMap<BlockId, Option<BlockId>>,
                children: &ViewMap<BlockId, BitMap<BlockId>>,
                frontiers: &mut ViewMap<BlockId, BitMap<BlockId>>,
            ) {
                for succ in tree.succs[key].iter() {
                    if matches!(idoms[succ], Some(idom) if idom != key) {
                        frontiers[key].insert(succ);
                    }
                }
                for child in children[key].iter() {
                    compute_df(child, tree, idoms, children, frontiers);
                    let mut frontier = frontiers[key].clone();
                    for front in frontiers[child].iter() {
                        if matches!(idoms[front], Some(idom) if idom != key) {
                            frontier.insert(front);
                        }
                    }
                    frontiers[key] = frontier;
                }
            }
            compute_df(entry, cfg, &idoms, &children, &mut frontiers);
            Self {
                dominators,
                idoms,
                children,
                frontiers,
            }
        }
    }
}

mod cfg {
    use std::collections::HashMap;

    use crate::{
        domain::{CfgBlock, Identifier, Terminator},
        map::{BitMap, BlockId, VecMap, ViewMap},
    };

    pub struct CfgTree {
        pub by_name: HashMap<Identifier, BlockId>,
        pub blocks: VecMap<BlockId, CfgBlock>,
        pub preds: ViewMap<BlockId, BitMap<BlockId>>,
        pub succs: ViewMap<BlockId, BitMap<BlockId>>,
        pub postorder: Vec<BlockId>,
    }
    impl CfgTree {
        pub fn postorder(&self) -> impl Iterator<Item = BlockId> + use<'_> {
            self.postorder.iter().copied()
        }
        pub fn reverse_postorder(&self) -> impl Iterator<Item = BlockId> + use<'_> {
            self.postorder.iter().copied().rev()
        }

        pub fn new(blocks_vec: Vec<CfgBlock>, entry: &Identifier) -> Self {
            let len = blocks_vec.len();
            let mut blocks = VecMap::with_capacity(len);
            let mut by_name = HashMap::with_capacity(len);

            for block in blocks_vec {
                let name = block.name.clone();
                let key = blocks.insert(block);
                by_name.insert(name, key);
            }

            let mut preds = blocks.bitmaps_zeros();
            let mut succs = blocks.bitmaps_zeros();

            for (key, block) in blocks.iter() {
                match &block.terminator {
                    Terminator::Jump(jump) => {
                        let target = by_name[&jump.target];
                        succs[key].insert(target);
                        preds[target].insert(key);
                    }
                    Terminator::Branch(branch) => {
                        let then_key = by_name[&branch.then];
                        succs[key].insert(then_key);
                        preds[then_key].insert(key);

                        let else_key = by_name[&branch.else_];
                        succs[key].insert(else_key);
                        preds[else_key].insert(key);
                    }
                    Terminator::Exit(_) => {}
                }
            }

            fn dfs(
                key: BlockId,
                succs: &ViewMap<BlockId, BitMap<BlockId>>,
                marks: &mut BitMap<BlockId>,
                postorder: &mut Vec<BlockId>,
            ) {
                if marks.insert(key) {
                    for succ_key in succs[key].iter() {
                        dfs(succ_key, succs, marks, postorder)
                    }
                    postorder.push(key);
                }
            }

            let mut marks = blocks.bitmap_zeros();
            let mut postorder = Vec::with_capacity(len);
            dfs(by_name[entry], &succs, &mut marks, &mut postorder);
            Self {
                by_name,
                blocks,
                preds,
                succs,
                postorder,
            }
        }
    }
}

// mod tree {
//     use std::collections::HashMap;

//     use bitvector::BitVector;
//     use slotmap::{new_key_type, Key, KeyData, SlotMap};

//     use crate::domain::{CfgBlock, Identifier, Terminator};

//     trait BitVectorExt {
//         fn insert_key<K: Key>(&mut self, key: K) -> bool;
//         fn contains_key<K: Key>(&self, key: K) -> bool;
//     }

//     fn idx(key: KeyData) -> usize {
//         (key.as_ffi() & 0xFFFF_FFFF) as usize
//     }
//     impl BitVectorExt for BitVector {
//         fn insert_key<K: Key>(&mut self, key: K) -> bool {
//             self.insert(idx(key.data()))
//         }
//         fn contains_key<K: Key>(&self, key: K) -> bool {
//             self.contains(idx(key.data()))
//         }
//     }

//     new_key_type! {
//         pub struct BlockId;
//     }
//     impl BlockId {}

//     pub struct CfgTree {
//         blocks: SlotMap<BlockId, CfgBlock>,
//         by_name: HashMap<Identifier, BlockId>,
//         postorder: Vec<BlockId>,
//         len: usize,
//     }
//     impl CfgTree {
//         pub fn len(&self) -> usize {
//             self.len
//         }
//         pub fn postorder(&self) -> &[BlockId] {
//             &self.postorder
//         }
//         pub fn get(&self, id: BlockId) -> &CfgBlock {
//             &self.blocks[id]
//         }
//         pub fn id(&self, name: &Identifier) -> BlockId {
//             self.by_name[name]
//         }

//         pub fn new(blocks_vec: Vec<CfgBlock>, entry: &Identifier) -> Self {
//             let len = blocks_vec.len();
//             let mut blocks: SlotMap<BlockId, _> = SlotMap::with_key();
//             let mut by_name = HashMap::new();

//             for block in blocks_vec {
//                 let name = block.name.clone();
//                 let id = blocks.insert(block);
//                 by_name.insert(name, id);
//             }

//             fn dfs(
//                 name: &Identifier,
//                 by_name: &HashMap<Identifier, BlockId>,
//                 blocks: &SlotMap<BlockId, CfgBlock>,
//                 marks: &mut BitVector,
//                 postorder: &mut Vec<BlockId>,
//             ) {
//                 let id = by_name[name];
//                 let blk = &blocks[id];
//                 if !marks.contains_key(id) {
//                     marks.insert_key(id);
//                     match &blk.terminator {
//                         Terminator::Jump(jump) => {
//                             dfs(&jump.target, by_name, blocks, marks, postorder)
//                         }
//                         Terminator::Branch(branch) => {
//                             dfs(&branch.then, by_name, blocks, marks, postorder);
//                             dfs(&branch.else_, by_name, blocks, marks, postorder);
//                         }
//                         Terminator::Exit(_) => {}
//                     };
//                     postorder.push(id);
//                 }
//             }

//             let mut marks = BitVector::new(len);
//             let mut postorder = Vec::with_capacity(len);
//             dfs(entry, &by_name, &blocks, &mut marks, &mut postorder);
//             Self {
//                 blocks,
//                 by_name,
//                 postorder,
//                 len,
//             }
//         }
//     }
// }

// mod tree {
//     use std::collections::HashMap;

//     use bitvector::BitVector;

//     use crate::domain::{CfgBlock, Identifier, Terminator};

//     pub struct CfgTree {
//         id_idx: HashMap<Identifier, usize>,
//         postorder: Vec<CfgBlock>,
//         len: usize,
//     }
//     impl CfgTree {
//         pub fn new(blocks: Vec<CfgBlock>, entry: &Identifier) -> Self {
//             type CfgById = HashMap<Identifier, (usize, CfgBlock)>;

//             let len = blocks.len();

//             let by_id: CfgById = blocks
//                 .into_iter()
//                 .enumerate()
//                 .map(|(idx, blk)| (blk.name.clone(), (idx, blk)))
//                 .collect();

//             fn dfs(
//                 blk: &Identifier,
//                 by_id: &CfgById,
//                 marks: &mut BitVector,
//                 postorder: &mut Vec<usize>,
//             ) {
//                 let (idx, blk) = &by_id[blk];
//                 if !marks.contains(*idx) {
//                     marks.insert(*idx);
//                     match &blk.terminator {
//                         Terminator::Jump(jump) => dfs(&jump.target, by_id, marks, postorder),
//                         Terminator::Branch(branch) => {
//                             dfs(&branch.then, by_id, marks, postorder);
//                             dfs(&branch.else_, by_id, marks, postorder);
//                         }
//                         Terminator::Exit(_) => {}
//                     };
//                     postorder.push(*idx);
//                 }
//             }

//             let mut marks = BitVector::new(len);
//             let mut postorder = Vec::with_capacity(len);
//             dfs(entry, &by_id, &mut marks, &mut postorder);

//             let mut by_old_idx: HashMap<_, _> = by_id
//                 .into_iter()
//                 .map(|(id, (idx, blk))| (idx, (id, blk)))
//                 .collect();
//             let (id_idx, postorder): (HashMap<_, _>, Vec<_>) = postorder
//                 .into_iter()
//                 .map(|idx| {
//                     let (id, blk) = by_old_idx.remove(&idx).unwrap();
//                     ((id, idx), blk)
//                 })
//                 .unzip();

//             Self {
//                 id_idx,
//                 postorder,
//                 len,
//             }
//         }

//         pub fn len(&self) -> usize {
//             self.len
//         }
//         pub fn idx(&self, id: &Identifier) -> usize {
//             self.id_idx[id]
//         }
//     }
// }
