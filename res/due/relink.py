#!/usr/bin/python3
import os
import shutil
import subprocess

Import("env")

def due_relink_after_build(target, source, env):
  bin_info = source[0]
  if bin_info.dir.name != "due":
    print("ez-clang: Skipping relink - it's only available for Arduino Due right now")
    return

  print("Relinking to expose stdlib functions...")
  make_env = os.environ.copy()

  # Primary toolchain is GCC
  gcc_binary_dir = make_env.get("GCC_BIN") or \
                   make_env["HOME"] + "/.platformio/packages/toolchain-gccarmnoneeabi/bin"
  if not os.path.isdir(gcc_binary_dir):
    print("Cannot find GCC binary directory:", gcc_binary_dir,
          "Please set your 'GCC_BIN' environment variable appropriately.")
    exit(1)

  # Makefile uses llvm-objcopy
  llvm_binary_dir = make_env.get("LLVM_BIN") or "/usr/lib/llvm-13/bin"
  if not os.path.isdir(llvm_binary_dir) or not os.path.isabs(llvm_binary_dir):
    print("Cannot find LLVM binary directory:", llvm_binary_dir,
          "Please set your 'LLVM_BIN' environment variable appropriately.")
    exit(1)

  # Makefile uses framework-arduino-sam
  arduino_dir = make_env.get("ARDUINO_SAM") or \
                make_env["HOME"] + "/.platformio/packages/framework-arduino-sam"
  if not os.path.isdir(arduino_dir) or not os.path.isabs(arduino_dir):
    print("Cannot find framework-arduino-sam:", arduino_dir,
          "Please set your 'ARDUINO_SAM' environment variable appropriately.")
    exit(1)

  # We will use existing build artifacts from platformio build root
  build_dir = os.path.abspath(".pio/build/" + bin_info.dir.name)
  if not os.path.isdir(build_dir):
    print("Cannot find platformio build root:", build_dir)
    exit(1)

  # We need extra inputs from the resources dir
  res_dir = os.path.abspath("res/" + bin_info.dir.name)
  if not os.path.isdir(res_dir):
    print("Cannot find resource dir:", res_dir)
    exit(1)

  # Dump section details from original ELF file
  original_elf = build_dir + "/firmware.elf"
  original_dump_cmd = [llvm_binary_dir + "/llvm-objdump", "-h", original_elf]
  if subprocess.Popen(original_dump_cmd).wait() != 0:
    print("Warning:", "llvm-objdump command failed:", original_dump_cmd)

  # Directory with relink tool and intermediate binaries
  tools_dir = os.path.abspath("./tools")
  relink_dir = os.path.abspath("./.relink/" + bin_info.dir.name)
  os.makedirs(relink_dir, exist_ok=True)

  # Add mandatory environment variables for make
  make_env.update({
    "HOST_CXX": "g++",
    "CXX": os.path.join(gcc_binary_dir, "arm-none-eabi-g++"),
    "NM": os.path.join(llvm_binary_dir, "llvm-nm"),
    "OBJCOPY": os.path.join(llvm_binary_dir, "llvm-objcopy"),
    "DEVICE_LIB_DIR": os.path.join(arduino_dir, "variants", "arduino_due_x"),
    "RELINK_DIR": relink_dir,
    "TOOLS_DIR": tools_dir,
    "BUILD_DIR": build_dir,
  })

  # Run Makefile in resource directory
  make_process = subprocess.Popen("make", cwd=res_dir, env=make_env,
                                          stderr=subprocess.PIPE,
                                          stdout=subprocess.PIPE)
  exit_code = make_process.wait()
  if exit_code != 0:
    print("Build failed in make:")
    out, err = make_process.communicate()
    print(str(out, "utf-8"))
    print(str(err, "utf-8"))
    exit(1)

  # Dump section details from ELF output file
  relink_elf = relink_dir + "/firmware.elf"
  relink_dump_cmd = [llvm_binary_dir + "/llvm-objdump", "-h", relink_elf]
  if subprocess.Popen(relink_dump_cmd).wait() != 0:
    print("Warning:", "llvm-objdump command failed:", relink_dump_cmd)

  # Replace firmware with relinked ELF
  relink_elf = relink_dir + "/firmware.elf"
  print("Replacing", original_elf)
  shutil.move(original_elf, original_elf + ".bak")
  shutil.move(relink_elf, original_elf)

  # Delete old firmware.bin in order to force platformio to regenerate it from
  # our relinked ELF
  final_bin = build_dir + "/firmware.bin"
  if os.path.isfile(final_bin):
    os.remove(final_bin)

  # Done
  print("Relink succeeded")

env.AddPostAction("checkprogsize", due_relink_after_build)
