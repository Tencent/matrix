/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <elf.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <unistd.h>

#include <unwindstack/DexFiles.h>
#include <unwindstack/Elf.h>
#include <unwindstack/JitDebug.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/Regs.h>
#include <unwindstack/Unwinder.h>

static bool Attach(pid_t pid) {
  if (ptrace(PTRACE_SEIZE, pid, 0, 0) == -1) {
    return false;
  }

  if (ptrace(PTRACE_INTERRUPT, pid, 0, 0) == -1) {
    ptrace(PTRACE_DETACH, pid, 0, 0);
    return false;
  }

  // Allow at least 1 second to attach properly.
  for (size_t i = 0; i < 1000; i++) {
    siginfo_t si;
    if (ptrace(PTRACE_GETSIGINFO, pid, 0, &si) == 0) {
      return true;
    }
    usleep(1000);
  }
  printf("%d: Failed to stop.\n", pid);
  return false;
}

void DoUnwind(pid_t pid) {
  unwindstack::Regs* regs = unwindstack::Regs::RemoteGet(pid);
  if (regs == nullptr) {
    printf("Unable to get remote reg data\n");
    return;
  }

  printf("ABI: ");
  switch (regs->Arch()) {
    case unwindstack::ARCH_ARM:
      printf("arm");
      break;
    case unwindstack::ARCH_X86:
      printf("x86");
      break;
    case unwindstack::ARCH_ARM64:
      printf("arm64");
      break;
    case unwindstack::ARCH_X86_64:
      printf("x86_64");
      break;
    case unwindstack::ARCH_MIPS:
      printf("mips");
      break;
    case unwindstack::ARCH_MIPS64:
      printf("mips64");
      break;
    default:
      printf("unknown\n");
      return;
  }
  printf("\n");

  unwindstack::UnwinderFromPid unwinder(1024, pid);
  if (!unwinder.Init(regs->Arch())) {
    printf("Failed to init unwinder object.\n");
    return;
  }

  unwinder.SetRegs(regs);
  unwinder.Unwind();

  // Print the frames.
  for (size_t i = 0; i < unwinder.NumFrames(); i++) {
    printf("%s\n", unwinder.FormatFrame(i).c_str());
  }
}

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("Usage: unwind <PID>\n");
    return 1;
  }

  pid_t pid = atoi(argv[1]);
  if (!Attach(pid)) {
    printf("Failed to attach to pid %d: %s\n", pid, strerror(errno));
    return 1;
  }

  DoUnwind(pid);

  ptrace(PTRACE_DETACH, pid, 0, 0);

  return 0;
}
