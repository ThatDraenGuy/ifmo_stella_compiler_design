use crate::{domain::SsaUnit, SirPass, SsaForm};

pub struct SccpPass;

impl SirPass for SccpPass {
    type In = SsaForm;

    type Out = SsaForm;

    fn perform(input: SsaUnit) -> SsaUnit {
        todo!()
    }
}
