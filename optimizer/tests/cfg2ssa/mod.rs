use std::{
    fs::{self, read_to_string},
    path::Path,
    process::Command,
};

use anyhow::anyhow;
use insta::{assert_snapshot, glob};
use pathdiff::diff_paths;
use stellac_optimizer::{domain::StellaIR, passes::Cfg2SsaPass, SirPass};

#[test]
fn test_cfg2ssa() {
    glob!("inputs/*.sir", |input_path| {
        let res = perform_test(input_path).unwrap();
        assert_snapshot!(res)
    })
}

fn perform_test(input_path_abs: &Path) -> Result<String, anyhow::Error> {
    let project_root = env!("CARGO_MANIFEST_DIR");
    let tempdir = tempfile::tempdir_in(project_root)?;

    let input_path = diff_paths(input_path_abs, project_root).unwrap();

    let json_input_abs = tempdir.path().join("input.json");
    let json_input = diff_paths(json_input_abs, project_root).unwrap();

    let json_output_abs = tempdir.path().join("output.json");
    let json_output = diff_paths(json_output_abs, project_root).unwrap();

    let sir_output_abs = tempdir.path().join("output.sir");
    let sir_output = diff_paths(sir_output_abs, project_root).unwrap();

    let cmd_res = Command::new("./stella")
        .arg("ir-convert")
        .arg(input_path)
        .arg("--from")
        .arg("sir")
        .arg("--to")
        .arg("json")
        .arg("-o")
        .arg(&json_input)
        .current_dir(project_root)
        .output()?;
    println!("{}", str::from_utf8(&cmd_res.stderr)?);
    if !cmd_res.status.success() {
        return Err(anyhow!(str::from_utf8(&cmd_res.stderr)?.to_owned()));
    }

    let input = read_to_string(&json_input)?;

    let ir: StellaIR = serde_json::from_str(&input)?;
    let StellaIR::CfgUnit(cfg) = ir else {
        return Err(anyhow!("Non CFG input"));
    };
    let ssa = Cfg2SsaPass::perform(cfg);
    let result_json = serde_json::to_string(&StellaIR::SsaUnit(ssa))?;

    fs::write(&json_output, result_json)?;

    let cmd_res = Command::new("./stella")
        .arg("ir-convert")
        .arg(&json_output)
        .arg("--from")
        .arg("json")
        .arg("--to")
        .arg("sir")
        .arg("-o")
        .arg(&sir_output)
        .current_dir(project_root)
        .output()?;
    if !cmd_res.status.success() {
        return Err(anyhow!(str::from_utf8(&cmd_res.stderr)?.to_owned()));
    }

    Ok(read_to_string(&sir_output)?)
}
