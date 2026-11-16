use crate::{domain::SsaUnit, SirPass, SsaForm};

pub struct DcePass;

impl SirPass for DcePass {
    type In = SsaForm;

    type Out = SsaForm;

    fn perform(input: SsaUnit) -> SsaUnit {
        todo!()
    }
}
