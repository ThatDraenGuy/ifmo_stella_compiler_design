use crate::{
    domain::{CfgUnit, SsaUnit},
    CfgForm, SirPass, SsaForm,
};

pub struct Ssa2CfgPass;

impl SirPass for Ssa2CfgPass {
    type In = SsaForm;

    type Out = CfgForm;

    fn perform(input: SsaUnit) -> CfgUnit {
        todo!()
    }
}
