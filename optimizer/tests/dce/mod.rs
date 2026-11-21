use std::path::Path;

use anyhow::anyhow;
use insta::{assert_snapshot, glob};
use stellac_optimizer::{domain::StellaIR, passes::DcePass, SirPass};

use crate::utils::wrap_test;

#[test]
fn test_dce() {
    glob!("inputs/*.sir", |input_path| {
        let res = perform_test(input_path).unwrap();
        assert_snapshot!(res)
    })
}

fn perform_test(input_path_abs: &Path) -> Result<String, anyhow::Error> {
    wrap_test(input_path_abs, |ir| {
        let StellaIR::SsaUnit(ssa) = ir else {
            return Err(anyhow!("Non SSA input"));
        };
        let ssa = DcePass::perform(ssa);
        Ok(StellaIR::SsaUnit(ssa))
    })
}
