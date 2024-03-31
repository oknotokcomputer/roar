use std::process::Command;

fn main() {
    // Run shell commands here
    run_shell_command("ls -l");
}

fn run_shell_command(command: &str) {
    let output = Command::new("sh")
        .arg("-c")
        .arg(command)
        .output()
        .expect("Failed to execute command");

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        println!("Command executed successfully:\n{}", stdout);
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        println!("Command failed:\n{}", stderr);
    }
}

// install cargo binary installs a cargo binary to the specified directory
fn install_cargo_binary() {
    let output = Command::new("cargo")
        .arg("install")
        .arg("--path")
        .arg(".")
        .output()
        .expect("Failed to execute command");

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        println!("Command executed successfully:\n{}", stdout);
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        println!("Command failed:\n{}", stderr);
    }
}