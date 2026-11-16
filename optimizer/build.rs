// Copyright 2023 Oxide Computer Company

use std::{env, fs, path::Path};

use typify::{TypeSpace, TypeSpacePatch, TypeSpaceSettings};

fn main() {
    let content = std::fs::read_to_string("ir/unit.schema.json").unwrap();
    let schema = serde_json::from_str::<schemars::schema::RootSchema>(&content).unwrap();

    let mut title_patch = TypeSpacePatch::default();
    title_patch.with_rename("StellaIR");
    let mut type_space = TypeSpace::new(
        TypeSpaceSettings::default()
            .with_patch("StellaIrUnitFormatVersion1", &title_patch)
            .map_typespace_settings(|s| s.with_struct_builder(true)),
    );
    type_space.add_root_schema(schema).unwrap();

    let contents =
        prettyplease::unparse(&syn::parse2::<syn::File>(type_space.to_stream().unwrap()).unwrap());

    let mut out_file = Path::new(&env::var("OUT_DIR").unwrap()).to_path_buf();
    out_file.push("codegen.rs");
    fs::write(out_file, contents).unwrap();
}
