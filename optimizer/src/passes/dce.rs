use std::{collections::HashMap, iter::once};

use crate::{
    domain::*,
    map::{BlockId, InstrId, VarId, VecMap, WorkQueue},
    SirPass, SsaForm,
};

pub struct DcePass;

impl SirPass for DcePass {
    type In = SsaForm;

    type Out = SsaForm;

    fn perform(input: SsaUnit) -> SsaUnit {
        SsaUnit {
            callables: input
                .callables
                .into_iter()
                .map(|callable| match callable {
                    SsaCallable::Function {
                        blocks,
                        entry,
                        name,
                        params,
                        result,
                    } => {
                        let ssa_blocks = perform_dce(blocks, &entry, &params);
                        SsaCallable::Function {
                            blocks: ssa_blocks,
                            entry,
                            name,
                            params,
                            result,
                        }
                    }
                    SsaCallable::Routine {
                        blocks,
                        entry,
                        name,
                        params,
                    } => {
                        let ssa_blocks = perform_dce(blocks, &entry, &params);
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
            form: input.form,
            format: input.format,
            frames: input.frames,
            layouts: input.layouts,
            profile: input.profile,
            version: input.version,
        }
    }
}

fn perform_dce(
    ssa_blocks: Vec<SsaBlock>,
    _entry: &Identifier,
    params: &[Binding],
) -> Vec<SsaBlock> {
    let mut blocks: VecMap<BlockId, _> = VecMap::new();
    let mut vars_by_name = HashMap::new();
    let mut vars: VecMap<VarId, _> = VecMap::new();
    let mut instrs: VecMap<InstrId, _> = VecMap::new();

    for param in params.iter() {
        let key = vars.insert(VarDef::Param);
        vars_by_name.insert(param.name.clone(), key);
    }

    for block in ssa_blocks {
        let blk_key = blocks.insert((block.name, block.terminator));
        for instr in block.instructions {
            let var_names: Vec<Identifier> = match &instr {
                SsaInstruction::Instruction(instruction) => match instruction {
                    Instruction::Scalar(scalar) => once(&scalar.results[0].name).cloned().collect(),
                    Instruction::Call(call) => once(&call.results[0].name).cloned().collect(),
                    Instruction::Run(run) => once(&run.results[0].name).cloned().collect(),
                    Instruction::Runtime(runtime) => runtime
                        .results
                        .iter()
                        .map(|res| &res.name)
                        .cloned()
                        .collect(),
                },
                SsaInstruction::Phi(phi) => once(&phi.results[0].name).cloned().collect(),
            };
            let key = instrs.insert(InstrDef { blk_key, instr });
            for var_name in var_names {
                let var_key = vars.insert(VarDef::Instr(key));
                vars_by_name.insert(var_name, var_key);
            }
        }
    }

    let mut dce_roots = instrs.bitmap_zeros();
    for (key, instr) in instrs.iter() {
        match &instr.instr {
            SsaInstruction::Instruction(instruction) => match instruction {
                Instruction::Scalar(scalar) => {
                    if matches!(scalar.op, ScalarOp::Div | ScalarOp::Rem)
                        && matches!(&scalar.args[1], Operand::I64(int) if int.as_str() != "0")
                    {
                        // div/rem с нелитарельным нулём во втором аргументе
                        dce_roots.insert(key);
                    }
                }
                _ => {
                    //call, run, runtime
                    dce_roots.insert(key);
                }
            },
            SsaInstruction::Phi(_phi) => {}
        }
    }

    let mut alive = instrs.bitmap_zeros();
    let mut work = instrs.work_queue(dce_roots.iter());

    let add_work = |op: &Operand, work: &mut WorkQueue<InstrId>| {
        if let Operand::Var(var) = op {
            if let VarDef::Instr(instr_key) = vars[vars_by_name[var]] {
                work.push_back(instr_key);
            }
        }
    };

    for (_blk_key, (_, terminator)) in blocks.iter() {
        match terminator {
            Terminator::Jump(_jump) => {}
            Terminator::Branch(branch) => add_work(&branch.args[0], &mut work),
            Terminator::Exit(exit) => match exit {
                Exit::Return(return_) => add_work(&return_.args[0], &mut work),
                Exit::Tail(tail) => {
                    for op in &tail.args {
                        add_work(op, &mut work);
                    }
                }
                Exit::TailEntry(tail_entry) => {
                    for op in &tail_entry.args {
                        add_work(op, &mut work);
                    }
                }
                Exit::Resume(resume) => add_work(&resume.args[0], &mut work),
                Exit::Fail(_fail) => {}
            },
        }
    }
    while let Some(key) = work.pop_front() {
        if alive.insert(key) {
            match &instrs[key].instr {
                SsaInstruction::Instruction(instruction) => {
                    let args = match instruction {
                        Instruction::Scalar(scalar) => &scalar.args,
                        Instruction::Call(call) => &call.args,
                        Instruction::Run(run) => &run.args,
                        Instruction::Runtime(runtime) => &runtime.args,
                    };
                    for arg in args {
                        add_work(arg, &mut work);
                    }
                }
                SsaInstruction::Phi(phi) => {
                    for item in &phi.incoming {
                        add_work(&item.value, &mut work);
                    }
                }
            }
        }
    }

    let mut blk_instrs = blocks.view_map(|_, _| Vec::new());
    for (key, instr) in instrs.drain() {
        if alive.contains(key) {
            blk_instrs[instr.blk_key].push(instr.instr);
        }
    }

    let mut result = Vec::new();
    for (key, (name, terminator)) in blocks.drain() {
        result.push(SsaBlock {
            instructions: blk_instrs[key].clone(),
            name,
            terminator,
        });
    }
    result
}

struct InstrDef {
    blk_key: BlockId,
    instr: SsaInstruction,
}
enum VarDef {
    Param,
    Instr(InstrId),
}
