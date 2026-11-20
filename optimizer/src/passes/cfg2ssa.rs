use std::{collections::HashMap, ops::Deref};

use crate::{
    domain::*,
    map::{BitMap, BlockId, VarId, VecMap, ViewMap},
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
                        let ssa_blocks = convert_to_ssa(blocks, &entry, locals, params.clone());
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
                        let ssa_blocks = convert_to_ssa(blocks, &entry, locals, params.clone());
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
    params: Vec<Binding>,
) -> Vec<SsaBlock> {
    let cfg = CfgTree::new(blocks, entry);
    let entry = cfg.by_name[entry];
    let dom = DomTree::new(&cfg, entry);
    let vars = Vars::new(locals, params, &cfg, &dom);

    //блок -> phi в его начале
    let mut blk_ssa_data = cfg.blocks.view_map(|_, _| SsaBlockData::default());
    //переменная -> блоки с её phi
    let mut var_phis = vars.vars.view_map(|_, _| cfg.blocks.bitmap_zeros());

    for (key, _bind) in vars.vars.iter() {
        let phis = &mut var_phis[key];
        let defs = &vars.defs[key];
        let mut work = cfg.blocks.work_queue(defs.iter());
        while let Some(blk) = work.pop_front() {
            for front in dom.frontiers[blk].iter() {
                if phis.insert(front) {
                    blk_ssa_data[front].phis.insert(key, PhiData::default());
                    if !defs.contains(front) {
                        work.push_back(front);
                    }
                }
            }
        }
    }

    fn rename_vars(
        blk: BlockId,
        cfg: &CfgTree,
        dom: &DomTree,
        vars: &Vars,
        blk_ssa_data: &mut ViewMap<BlockId, SsaBlockData>,
        name_counters: &mut ViewMap<VarId, UndoCounter>,
    ) {
        //сохранить счётчики
        for (_, counter) in name_counters.iter_mut() {
            counter.save();
        }

        //вспомогательные функции
        fn rename_id_read(
            id: &Identifier,
            vars: &Vars,
            name_counters: &mut ViewMap<VarId, UndoCounter>,
        ) -> Identifier {
            let idx = name_counters[vars.by_name[id]].get();
            if idx == 0 {
                id.clone()
            } else {
                Identifier::try_from(format!("{}_{}", id.deref(), idx)).unwrap()
            }
        }
        fn rename_id_write(
            id: &Identifier,
            vars: &Vars,
            name_counters: &mut ViewMap<VarId, UndoCounter>,
        ) -> Identifier {
            Identifier::try_from(format!(
                "{}_{}",
                id.deref(),
                name_counters[vars.by_name[id]].inc()
            ))
            .unwrap()
        }
        fn rename_operand(
            arg: &Operand,
            vars: &Vars,
            name_counters: &mut ViewMap<VarId, UndoCounter>,
        ) -> Operand {
            match arg {
                Operand::Var(id) => rename_id_read(id, vars, name_counters).into(),
                Operand::I64(int) => int.clone().into(),
                Operand::Bool(b) => (*b).into(),
            }
        }
        fn rename_args(
            args: &[Operand],
            vars: &Vars,
            name_counters: &mut ViewMap<VarId, UndoCounter>,
        ) -> Vec<Operand> {
            args.iter()
                .map(|arg| rename_operand(arg, vars, name_counters))
                .collect()
        }

        //заполнить номера в результатах phi
        for (var_key, phi) in blk_ssa_data[blk].phis.iter_mut() {
            let var_def = &vars.vars[*var_key].bind;
            phi.res = Some(Binding {
                name: rename_id_write(&var_def.name, vars, name_counters),
                type_: var_def.type_,
            })
        }

        //заполнить номера в чтениях и записях
        for instr in &cfg.blocks[blk].instructions {
            let new_instr: Instruction = match instr {
                Instruction::Scalar(scalar) => Scalar {
                    args: rename_args(&scalar.args, vars, name_counters),
                    op: scalar.op,
                    results: [Binding {
                        name: rename_id_write(&scalar.results[0].name, vars, name_counters),
                        type_: scalar.results[0].type_,
                    }],
                }
                .into(),
                Instruction::Call(call) => Call {
                    args: rename_args(&call.args, vars, name_counters),
                    callee: call.callee.clone(),
                    op: call.op.clone(),
                    results: [Binding {
                        name: rename_id_write(&call.results[0].name, vars, name_counters),
                        type_: call.results[0].type_,
                    }],
                }
                .into(),
                Instruction::Run(run) => Run {
                    args: rename_args(&run.args, vars, name_counters),
                    op: run.op.clone(),
                    results: [Binding {
                        name: rename_id_write(&run.results[0].name, vars, name_counters),
                        type_: run.results[0].type_,
                    }],
                    target: run.target.clone(),
                }
                .into(),
                Instruction::Runtime(runtime) => Runtime {
                    args: rename_args(&runtime.args, vars, name_counters),
                    attributes: runtime.attributes.clone(),
                    name: runtime.name.clone(),
                    op: runtime.op.clone(),
                    results: runtime
                        .results
                        .iter()
                        .map(|binding| Binding {
                            name: rename_id_write(&binding.name, vars, name_counters),
                            type_: binding.type_,
                        })
                        .collect(),
                }
                .into(),
            };
            blk_ssa_data[blk].instructions.push(new_instr);
        }
        blk_ssa_data[blk].terminator = Some(match &cfg.blocks[blk].terminator {
            Terminator::Jump(jump) => jump.clone().into(),
            Terminator::Branch(branch) => Branch {
                args: [rename_operand(&branch.args[0], vars, name_counters)],
                else_: branch.else_.clone(),
                op: branch.op.clone(),
                then: branch.then.clone(),
            }
            .into(),
            Terminator::Exit(exit) => <Terminator as From<Exit>>::from(match exit {
                Exit::Return(return_) => Return {
                    args: [rename_operand(&return_.args[0], vars, name_counters)],
                    op: return_.op.clone(),
                }
                .into(),
                Exit::Tail(tail) => Tail {
                    args: rename_args(&tail.args, vars, name_counters),
                    op: tail.op.clone(),
                    target: tail.target.clone(),
                }
                .into(),
                Exit::TailEntry(tail_entry) => TailEntry {
                    args: rename_args(&tail_entry.args, vars, name_counters),
                    op: tail_entry.op.clone(),
                    signature: tail_entry.signature.clone(),
                }
                .into(),
                Exit::Resume(resume) => Resume {
                    args: [rename_operand(&resume.args[0], vars, name_counters)],
                    op: resume.op.clone(),
                }
                .into(),
                Exit::Fail(fail) => Fail {
                    error: fail.error.clone(),
                    op: fail.op.clone(),
                }
                .into(),
            }),
        });

        //заполнить аргумент phi наследников
        for succ in cfg.succs[blk].iter() {
            for (var_key, phi) in blk_ssa_data[succ].phis.iter_mut() {
                let var_id = &vars.vars[*var_key].bind.name;
                if name_counters[*var_key].get() == 0 && !vars.vars[*var_key].is_param {
                    //dead phi
                    phi.args.push(PhiArg::Dead);
                } else {
                    phi.args.push(PhiArg::Item(PhiIncomingItem {
                        pred: cfg.blocks[blk].name.clone(),
                        value: Operand::Var(rename_id_read(var_id, vars, name_counters)),
                    }));
                }
            }
        }

        //переименовать в детях
        for child in dom.children[blk].iter() {
            rename_vars(child, cfg, dom, vars, blk_ssa_data, name_counters);
        }

        //восстановить счётчики
        for (_, stack) in name_counters.iter_mut() {
            stack.rollback();
        }
    }

    let mut name_counters = vars.vars.view_map(|_, _| UndoCounter::new());
    rename_vars(
        entry,
        &cfg,
        &dom,
        &vars,
        &mut blk_ssa_data,
        &mut name_counters,
    );

    cfg.reverse_postorder()
        .map(|blk| {
            let ssa_data = &mut blk_ssa_data[blk];
            let mut instructions = Vec::new();
            for (_var_key, phi) in ssa_data.phis.drain() {
                if phi.args.iter().all(|arg| matches!(arg, PhiArg::Item(_))) {
                    instructions.push(SsaInstruction::Phi(Phi {
                        incoming: phi.args.into_iter().map(PhiArg::unwrap_item).collect(),
                        op: serde_json::Value::String("phi".to_owned()),
                        results: [phi.res.unwrap()],
                    }));
                }
            }
            for instr in ssa_data.instructions.drain(..) {
                instructions.push(SsaInstruction::Instruction(instr));
            }
            return SsaBlock {
                instructions,
                name: cfg.blocks[blk].name.clone(),
                terminator: ssa_data.terminator.take().unwrap(),
            };
        })
        .collect()
}

#[derive(Default)]
struct SsaBlockData {
    phis: HashMap<VarId, PhiData>,
    instructions: Vec<Instruction>,
    terminator: Option<Terminator>,
}

enum PhiArg {
    Dead,
    Item(PhiIncomingItem),
}
impl PhiArg {
    fn unwrap_item(self) -> PhiIncomingItem {
        match self {
            PhiArg::Dead => panic!(),
            PhiArg::Item(item) => item,
        }
    }
}

#[derive(Default)]
struct PhiData {
    res: Option<Binding>,
    args: Vec<PhiArg>,
}

struct UndoCounter {
    curr: usize,
    max: usize,
    checkpoints: Vec<usize>,
}
impl UndoCounter {
    pub fn new() -> Self {
        Self {
            curr: 0,
            max: 0,
            checkpoints: Vec::new(),
        }
    }
    pub fn inc(&mut self) -> usize {
        self.max += 1;
        self.curr = self.max;
        self.curr
    }
    pub fn get(&self) -> usize {
        self.curr
    }

    pub fn save(&mut self) {
        self.checkpoints.push(self.curr);
    }
    pub fn rollback(&mut self) {
        if let Some(len) = self.checkpoints.pop() {
            self.curr = len;
        }
    }
}

struct Vars {
    by_name: HashMap<Identifier, VarId>,
    vars: VecMap<VarId, VarDef>,
    defs: ViewMap<VarId, BitMap<BlockId>>,
}
struct VarDef {
    bind: Binding,
    is_param: bool,
}
impl Vars {
    fn new(locals: Vec<Binding>, params: Vec<Binding>, cfg: &CfgTree, _dom: &DomTree) -> Self {
        let len = locals.len() + params.len();
        let mut by_name = HashMap::with_capacity(len);
        let mut vars = VecMap::with_capacity(len);
        for bind in locals {
            let name = bind.name.clone();
            let key = vars.insert(VarDef {
                bind,
                is_param: false,
            });
            by_name.insert(name, key);
        }
        for bind in params {
            let name = bind.name.clone();
            let key = vars.insert(VarDef {
                bind,
                is_param: true,
            });
            by_name.insert(name, key);
        }

        let mut defs = vars.view_map(|_key, _var| cfg.blocks.bitmap_zeros());
        for (blk_key, blk) in cfg.blocks.iter() {
            for instr in &blk.instructions {
                let results: &[Binding] = match instr {
                    Instruction::Scalar(scalar) => &scalar.results,
                    Instruction::Call(call) => &call.results,
                    Instruction::Run(run) => &run.results,
                    Instruction::Runtime(runtime) => &runtime.results,
                };
                for res in results {
                    let key = by_name[&res.name];
                    defs[key].insert(blk_key);
                }
            }
        }

        Self {
            by_name,
            vars,
            defs,
        }
    }
}

struct DomTree {
    dominators: ViewMap<BlockId, BitMap<BlockId>>,
    idoms: ViewMap<BlockId, Option<BlockId>>,
    children: ViewMap<BlockId, BitMap<BlockId>>,
    frontiers: ViewMap<BlockId, BitMap<BlockId>>,
}
impl DomTree {
    fn new(cfg: &CfgTree, entry: BlockId) -> Self {
        let mut dominators = cfg.blocks.bitmaps_ones();
        dominators[entry].clear();
        dominators[entry].insert(entry);

        let mut work = cfg.blocks.work_queue(cfg.reverse_postorder());
        while let Some(key) = work.pop_front() {
            let mut doms = dominators[key].clone();
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

struct CfgTree {
    by_name: HashMap<Identifier, BlockId>,
    blocks: VecMap<BlockId, CfgBlock>,
    preds: ViewMap<BlockId, BitMap<BlockId>>,
    succs: ViewMap<BlockId, BitMap<BlockId>>,
    postorder: Vec<BlockId>,
}
impl CfgTree {
    fn postorder(&self) -> impl Iterator<Item = BlockId> + use<'_> {
        self.postorder.iter().copied()
    }
    fn reverse_postorder(&self) -> impl Iterator<Item = BlockId> + use<'_> {
        self.postorder.iter().copied().rev()
    }

    fn new(blocks_vec: Vec<CfgBlock>, entry: &Identifier) -> Self {
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
