use std::path::Path;

use anyhow::anyhow;
use insta::{assert_snapshot, glob};
use stellac_optimizer::{domain::StellaIR, passes::Cfg2SsaPass, SirPass};

use crate::utils::wrap_test;

#[test]
fn test_cfg2ssa() {
    glob!("inputs/*.sir", |input_path| {
        let res = perform_test(input_path).unwrap();
        assert_snapshot!(res)
    })
}

fn perform_test(input_path_abs: &Path) -> Result<String, anyhow::Error> {
    wrap_test(input_path_abs, |ir| {
        let StellaIR::CfgUnit(cfg) = ir else {
            return Err(anyhow!("Non CFG input"));
        };
        let ssa = Cfg2SsaPass::perform(cfg);
        Ok(StellaIR::SsaUnit(ssa))
    })
}
