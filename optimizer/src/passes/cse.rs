use crate::{domain::SsaUnit, SirPass, SsaForm};

pub struct CsePass;

impl SirPass for CsePass {
    type In = SsaForm;

    type Out = SsaForm;

    fn perform(input: SsaUnit) -> SsaUnit {
        todo!()
    }
}
