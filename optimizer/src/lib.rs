pub mod domain;
pub mod map;
pub mod passes;

use crate::domain::{CfgUnit, SsaUnit};

pub trait SirForm {
    type Unit;
}

pub struct CfgForm;
impl SirForm for CfgForm {
    type Unit = CfgUnit;
}

pub struct SsaForm;
impl SirForm for SsaForm {
    type Unit = SsaUnit;
}

pub trait SirPass {
    type In: SirForm;
    type Out: SirForm;

    fn perform(input: <Self::In as SirForm>::Unit) -> <Self::Out as SirForm>::Unit;
}
