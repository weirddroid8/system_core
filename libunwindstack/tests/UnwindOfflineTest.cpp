/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <vector>

#include <android-base/file.h>

#include <unwindstack/JitDebug.h>
#include <unwindstack/MachineArm.h>
#include <unwindstack/MachineArm64.h>
#include <unwindstack/MachineX86.h>
#include <unwindstack/MachineX86_64.h>
#include <unwindstack/Maps.h>
#include <unwindstack/Memory.h>
#include <unwindstack/RegsArm.h>
#include <unwindstack/RegsArm64.h>
#include <unwindstack/RegsX86.h>
#include <unwindstack/RegsX86_64.h>
#include <unwindstack/Unwinder.h>

#include "ElfTestUtils.h"

namespace unwindstack {

class UnwindOfflineTest : public ::testing::Test {
 protected:
  void TearDown() override {
    if (cwd_ != nullptr) {
      ASSERT_EQ(0, chdir(cwd_));
    }
    free(cwd_);
  }

  void Init(const char* file_dir, ArchEnum arch) {
    dir_ = TestGetFileDirectory() + "offline/" + file_dir;

    std::string data;
    ASSERT_TRUE(android::base::ReadFileToString((dir_ + "maps.txt"), &data));

    maps_.reset(new BufferMaps(data.c_str()));
    ASSERT_TRUE(maps_->Parse());

    std::unique_ptr<MemoryOffline> stack_memory(new MemoryOffline);
    ASSERT_TRUE(stack_memory->Init((dir_ + "stack.data").c_str(), 0));
    process_memory_.reset(stack_memory.release());

    switch (arch) {
      case ARCH_ARM: {
        RegsArm* regs = new RegsArm;
        regs_.reset(regs);
        ReadRegs<uint32_t>(regs, arm_regs_);
        break;
      }
      case ARCH_ARM64: {
        RegsArm64* regs = new RegsArm64;
        regs_.reset(regs);
        ReadRegs<uint64_t>(regs, arm64_regs_);
        break;
      }
      case ARCH_X86: {
        RegsX86* regs = new RegsX86;
        regs_.reset(regs);
        ReadRegs<uint32_t>(regs, x86_regs_);
        break;
      }
      case ARCH_X86_64: {
        RegsX86_64* regs = new RegsX86_64;
        regs_.reset(regs);
        ReadRegs<uint64_t>(regs, x86_64_regs_);
        break;
      }
      default:
        ASSERT_TRUE(false) << "Unknown arch " << std::to_string(arch);
    }
    cwd_ = getcwd(nullptr, 0);
    // Make dir_ an absolute directory.
    if (dir_.empty() || dir_[0] != '/') {
      dir_ = std::string(cwd_) + '/' + dir_;
    }
    ASSERT_EQ(0, chdir(dir_.c_str()));
  }

  template <typename AddressType>
  void ReadRegs(RegsImpl<AddressType>* regs,
                const std::unordered_map<std::string, uint32_t>& name_to_reg) {
    FILE* fp = fopen((dir_ + "regs.txt").c_str(), "r");
    ASSERT_TRUE(fp != nullptr);
    while (!feof(fp)) {
      uint64_t value;
      char reg_name[100];
      ASSERT_EQ(2, fscanf(fp, "%s %" SCNx64 "\n", reg_name, &value));
      std::string name(reg_name);
      if (!name.empty()) {
        // Remove the : from the end.
        name.resize(name.size() - 1);
      }
      auto entry = name_to_reg.find(name);
      ASSERT_TRUE(entry != name_to_reg.end()) << "Unknown register named " << name;
      (*regs)[entry->second] = value;
    }
    fclose(fp);
    regs->SetFromRaw();
  }

  static std::unordered_map<std::string, uint32_t> arm_regs_;
  static std::unordered_map<std::string, uint32_t> arm64_regs_;
  static std::unordered_map<std::string, uint32_t> x86_regs_;
  static std::unordered_map<std::string, uint32_t> x86_64_regs_;

  char* cwd_ = nullptr;
  std::string dir_;
  std::unique_ptr<Regs> regs_;
  std::unique_ptr<Maps> maps_;
  std::shared_ptr<Memory> process_memory_;
};

std::unordered_map<std::string, uint32_t> UnwindOfflineTest::arm_regs_ = {
    {"r0", ARM_REG_R0},  {"r1", ARM_REG_R1}, {"r2", ARM_REG_R2},   {"r3", ARM_REG_R3},
    {"r4", ARM_REG_R4},  {"r5", ARM_REG_R5}, {"r6", ARM_REG_R6},   {"r7", ARM_REG_R7},
    {"r8", ARM_REG_R8},  {"r9", ARM_REG_R9}, {"r10", ARM_REG_R10}, {"r11", ARM_REG_R11},
    {"ip", ARM_REG_R12}, {"sp", ARM_REG_SP}, {"lr", ARM_REG_LR},   {"pc", ARM_REG_PC},
};

std::unordered_map<std::string, uint32_t> UnwindOfflineTest::arm64_regs_ = {
    {"x0", ARM64_REG_R0},   {"x1", ARM64_REG_R1},   {"x2", ARM64_REG_R2},   {"x3", ARM64_REG_R3},
    {"x4", ARM64_REG_R4},   {"x5", ARM64_REG_R5},   {"x6", ARM64_REG_R6},   {"x7", ARM64_REG_R7},
    {"x8", ARM64_REG_R8},   {"x9", ARM64_REG_R9},   {"x10", ARM64_REG_R10}, {"x11", ARM64_REG_R11},
    {"x12", ARM64_REG_R12}, {"x13", ARM64_REG_R13}, {"x14", ARM64_REG_R14}, {"x15", ARM64_REG_R15},
    {"x16", ARM64_REG_R16}, {"x17", ARM64_REG_R17}, {"x18", ARM64_REG_R18}, {"x19", ARM64_REG_R19},
    {"x20", ARM64_REG_R20}, {"x21", ARM64_REG_R21}, {"x22", ARM64_REG_R22}, {"x23", ARM64_REG_R23},
    {"x24", ARM64_REG_R24}, {"x25", ARM64_REG_R25}, {"x26", ARM64_REG_R26}, {"x27", ARM64_REG_R27},
    {"x28", ARM64_REG_R28}, {"x29", ARM64_REG_R29}, {"sp", ARM64_REG_SP},   {"lr", ARM64_REG_LR},
    {"pc", ARM64_REG_PC},
};

std::unordered_map<std::string, uint32_t> UnwindOfflineTest::x86_regs_ = {
    {"eax", X86_REG_EAX}, {"ebx", X86_REG_EBX}, {"ecx", X86_REG_ECX},
    {"edx", X86_REG_EDX}, {"ebp", X86_REG_EBP}, {"edi", X86_REG_EDI},
    {"esi", X86_REG_ESI}, {"esp", X86_REG_ESP}, {"eip", X86_REG_EIP},
};

std::unordered_map<std::string, uint32_t> UnwindOfflineTest::x86_64_regs_ = {
    {"rax", X86_64_REG_RAX}, {"rbx", X86_64_REG_RBX}, {"rcx", X86_64_REG_RCX},
    {"rdx", X86_64_REG_RDX}, {"r8", X86_64_REG_R8},   {"r9", X86_64_REG_R9},
    {"r10", X86_64_REG_R10}, {"r11", X86_64_REG_R11}, {"r12", X86_64_REG_R12},
    {"r13", X86_64_REG_R13}, {"r14", X86_64_REG_R14}, {"r15", X86_64_REG_R15},
    {"rdi", X86_64_REG_RDI}, {"rsi", X86_64_REG_RSI}, {"rbp", X86_64_REG_RBP},
    {"rsp", X86_64_REG_RSP}, {"rip", X86_64_REG_RIP},
};

static std::string DumpFrames(Unwinder& unwinder) {
  std::string str;
  for (size_t i = 0; i < unwinder.NumFrames(); i++) {
    str += unwinder.FormatFrame(i) + "\n";
  }
  return str;
}

TEST_F(UnwindOfflineTest, pc_straddle_arm) {
  Init("straddle_arm/", ARCH_ARM);

  Unwinder unwinder(128, maps_.get(), regs_.get(), process_memory_);
  unwinder.Unwind();

  std::string frame_info(DumpFrames(unwinder));
  ASSERT_EQ(4U, unwinder.NumFrames()) << "Unwind:\n" << frame_info;
  EXPECT_EQ(
      "  #00 pc 0001a9f8  libc.so (abort+63)\n"
      "  #01 pc 00006a1b  libbase.so (_ZN7android4base14DefaultAborterEPKc+6)\n"
      "  #02 pc 00007441  libbase.so (_ZN7android4base10LogMessageD2Ev+748)\n"
      "  #03 pc 00015147  /does/not/exist/libhidlbase.so\n",
      frame_info);
}

TEST_F(UnwindOfflineTest, pc_in_gnu_debugdata_arm) {
  Init("gnu_debugdata_arm/", ARCH_ARM);

  Unwinder unwinder(128, maps_.get(), regs_.get(), process_memory_);
  unwinder.Unwind();

  std::string frame_info(DumpFrames(unwinder));
  ASSERT_EQ(2U, unwinder.NumFrames()) << "Unwind:\n" << frame_info;
  EXPECT_EQ(
      "  #00 pc 0006dc49  libandroid_runtime.so "
      "(_ZN7android14AndroidRuntime15javaThreadShellEPv+80)\n"
      "  #01 pc 0006dce5  libandroid_runtime.so "
      "(_ZN7android14AndroidRuntime19javaCreateThreadEtcEPFiPvES1_PKcijPS1_)\n",
      frame_info);
}

TEST_F(UnwindOfflineTest, pc_straddle_arm64) {
  Init("straddle_arm64/", ARCH_ARM64);

  Unwinder unwinder(128, maps_.get(), regs_.get(), process_memory_);
  unwinder.Unwind();

  std::string frame_info(DumpFrames(unwinder));
  ASSERT_EQ(6U, unwinder.NumFrames()) << "Unwind:\n" << frame_info;
  EXPECT_EQ(
      "  #00 pc 0000000000429fd8  libunwindstack_test (SignalInnerFunction+24)\n"
      "  #01 pc 000000000042a078  libunwindstack_test (SignalMiddleFunction+8)\n"
      "  #02 pc 000000000042a08c  libunwindstack_test (SignalOuterFunction+8)\n"
      "  #03 pc 000000000042d8fc  libunwindstack_test "
      "(_ZN11unwindstackL19RemoteThroughSignalEij+20)\n"
      "  #04 pc 000000000042d8d8  libunwindstack_test "
      "(_ZN11unwindstack37UnwindTest_remote_through_signal_Test8TestBodyEv+32)\n"
      "  #05 pc 0000000000455d70  libunwindstack_test (_ZN7testing4Test3RunEv+392)\n",
      frame_info);
}

static void AddMemory(std::string file_name, MemoryOfflineParts* parts) {
  MemoryOffline* memory = new MemoryOffline;
  ASSERT_TRUE(memory->Init(file_name.c_str(), 0));
  parts->Add(memory);
}

TEST_F(UnwindOfflineTest, jit_debug_x86) {
  Init("jit_debug_x86/", ARCH_X86);

  MemoryOfflineParts* memory = new MemoryOfflineParts;
  AddMemory(dir_ + "descriptor.data", memory);
  AddMemory(dir_ + "stack.data", memory);
  for (size_t i = 0; i < 7; i++) {
    AddMemory(dir_ + "entry" + std::to_string(i) + ".data", memory);
    AddMemory(dir_ + "jit" + std::to_string(i) + ".data", memory);
  }
  process_memory_.reset(memory);

  JitDebug jit_debug(process_memory_);
  Unwinder unwinder(128, maps_.get(), regs_.get(), process_memory_);
  unwinder.SetJitDebug(&jit_debug, regs_->Arch());
  unwinder.Unwind();

  std::string frame_info(DumpFrames(unwinder));
  ASSERT_EQ(69U, unwinder.NumFrames()) << "Unwind:\n" << frame_info;
  EXPECT_EQ(
      "  #00 pc 00068fb8  libarttestd.so (_ZN3artL13CauseSegfaultEv+72)\n"
      "  #01 pc 00067f00  libarttestd.so (Java_Main_unwindInProcess+10032)\n"
      "  #02 pc 000021a8 (offset 0x2000)  137-cfi.odex (boolean Main.unwindInProcess(boolean, int, "
      "boolean)+136)\n"
      "  #03 pc 0000fe81  anonymous:ee74c000 (boolean Main.bar(boolean)+65)\n"
      "  #04 pc 006ad4d2  libartd.so (art_quick_invoke_stub+338)\n"
      "  #05 pc 00146ab5  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+885)\n"
      "  #06 pc 0039cf0d  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+653)\n"
      "  #07 pc 00392552  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+354)\n"
      "  #08 pc 0039399a  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+234)\n"
      "  #09 pc 00684362  libartd.so (artQuickToInterpreterBridge+1058)\n"
      "  #10 pc 006b35bd  libartd.so (art_quick_to_interpreter_bridge+77)\n"
      "  #11 pc 0000fe04  anonymous:ee74c000 (int Main.compare(Main, Main)+52)\n"
      "  #12 pc 006ad4d2  libartd.so (art_quick_invoke_stub+338)\n"
      "  #13 pc 00146ab5  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+885)\n"
      "  #14 pc 0039cf0d  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+653)\n"
      "  #15 pc 00392552  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+354)\n"
      "  #16 pc 0039399a  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+234)\n"
      "  #17 pc 00684362  libartd.so (artQuickToInterpreterBridge+1058)\n"
      "  #18 pc 006b35bd  libartd.so (art_quick_to_interpreter_bridge+77)\n"
      "  #19 pc 0000fd3c  anonymous:ee74c000 (int Main.compare(java.lang.Object, "
      "java.lang.Object)+108)\n"
      "  #20 pc 006ad4d2  libartd.so (art_quick_invoke_stub+338)\n"
      "  #21 pc 00146ab5  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+885)\n"
      "  #22 pc 0039cf0d  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+653)\n"
      "  #23 pc 00392552  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+354)\n"
      "  #24 pc 0039399a  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+234)\n"
      "  #25 pc 00684362  libartd.so (artQuickToInterpreterBridge+1058)\n"
      "  #26 pc 006b35bd  libartd.so (art_quick_to_interpreter_bridge+77)\n"
      "  #27 pc 0000fbdc  anonymous:ee74c000 (int "
      "java.util.Arrays.binarySearch0(java.lang.Object[], int, int, java.lang.Object, "
      "java.util.Comparator)+332)\n"
      "  #28 pc 006ad6a2  libartd.so (art_quick_invoke_static_stub+418)\n"
      "  #29 pc 00146acb  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+907)\n"
      "  #30 pc 0039cf0d  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+653)\n"
      "  #31 pc 00392552  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+354)\n"
      "  #32 pc 0039399a  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+234)\n"
      "  #33 pc 00684362  libartd.so (artQuickToInterpreterBridge+1058)\n"
      "  #34 pc 006b35bd  libartd.so (art_quick_to_interpreter_bridge+77)\n"
      "  #35 pc 0000f625  anonymous:ee74c000 (boolean Main.foo()+165)\n"
      "  #36 pc 006ad4d2  libartd.so (art_quick_invoke_stub+338)\n"
      "  #37 pc 00146ab5  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+885)\n"
      "  #38 pc 0039cf0d  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+653)\n"
      "  #39 pc 00392552  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+354)\n"
      "  #40 pc 0039399a  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+234)\n"
      "  #41 pc 00684362  libartd.so (artQuickToInterpreterBridge+1058)\n"
      "  #42 pc 006b35bd  libartd.so (art_quick_to_interpreter_bridge+77)\n"
      "  #43 pc 0000eedc  anonymous:ee74c000 (void Main.runPrimary()+60)\n"
      "  #44 pc 006ad4d2  libartd.so (art_quick_invoke_stub+338)\n"
      "  #45 pc 00146ab5  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+885)\n"
      "  #46 pc 0039cf0d  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+653)\n"
      "  #47 pc 00392552  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+354)\n"
      "  #48 pc 0039399a  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+234)\n"
      "  #49 pc 00684362  libartd.so (artQuickToInterpreterBridge+1058)\n"
      "  #50 pc 006b35bd  libartd.so (art_quick_to_interpreter_bridge+77)\n"
      "  #51 pc 0000ac22  anonymous:ee74c000 (void Main.main(java.lang.String[])+98)\n"
      "  #52 pc 006ad6a2  libartd.so (art_quick_invoke_static_stub+418)\n"
      "  #53 pc 00146acb  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+907)\n"
      "  #54 pc 0039cf0d  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+653)\n"
      "  #55 pc 00392552  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+354)\n"
      "  #56 pc 0039399a  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+234)\n"
      "  #57 pc 00684362  libartd.so (artQuickToInterpreterBridge+1058)\n"
      "  #58 pc 006b35bd  libartd.so (art_quick_to_interpreter_bridge+77)\n"
      "  #59 pc 006ad6a2  libartd.so (art_quick_invoke_static_stub+418)\n"
      "  #60 pc 00146acb  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+907)\n"
      "  #61 pc 005aac95  libartd.so "
      "(_ZN3artL18InvokeWithArgArrayERKNS_33ScopedObjectAccessAlreadyRunnableEPNS_9ArtMethodEPNS_"
      "8ArgArrayEPNS_6JValueEPKc+85)\n"
      "  #62 pc 005aab5a  libartd.so "
      "(_ZN3art17InvokeWithVarArgsERKNS_33ScopedObjectAccessAlreadyRunnableEP8_jobjectP10_"
      "jmethodIDPc+362)\n"
      "  #63 pc 0048a3dd  libartd.so "
      "(_ZN3art3JNI21CallStaticVoidMethodVEP7_JNIEnvP7_jclassP10_jmethodIDPc+125)\n"
      "  #64 pc 0018448c  libartd.so "
      "(_ZN3art8CheckJNI11CallMethodVEPKcP7_JNIEnvP8_jobjectP7_jclassP10_jmethodIDPcNS_"
      "9Primitive4TypeENS_10InvokeTypeE+1964)\n"
      "  #65 pc 0017cf06  libartd.so "
      "(_ZN3art8CheckJNI21CallStaticVoidMethodVEP7_JNIEnvP7_jclassP10_jmethodIDPc+70)\n"
      "  #66 pc 00001d8c  dalvikvm32 "
      "(_ZN7_JNIEnv20CallStaticVoidMethodEP7_jclassP10_jmethodIDz+60)\n"
      "  #67 pc 00001a80  dalvikvm32 (main+1312)\n"
      "  #68 pc 00018275  libc.so\n",
      frame_info);
}

TEST_F(UnwindOfflineTest, jit_debug_arm) {
  Init("jit_debug_arm/", ARCH_ARM);

  MemoryOfflineParts* memory = new MemoryOfflineParts;
  AddMemory(dir_ + "descriptor.data", memory);
  AddMemory(dir_ + "descriptor1.data", memory);
  AddMemory(dir_ + "stack.data", memory);
  for (size_t i = 0; i < 7; i++) {
    AddMemory(dir_ + "entry" + std::to_string(i) + ".data", memory);
    AddMemory(dir_ + "jit" + std::to_string(i) + ".data", memory);
  }
  process_memory_.reset(memory);

  JitDebug jit_debug(process_memory_);
  Unwinder unwinder(128, maps_.get(), regs_.get(), process_memory_);
  unwinder.SetJitDebug(&jit_debug, regs_->Arch());
  unwinder.Unwind();

  std::string frame_info(DumpFrames(unwinder));
  ASSERT_EQ(76U, unwinder.NumFrames()) << "Unwind:\n" << frame_info;
  EXPECT_EQ(
      "  #00 pc 00018a5e  libarttestd.so (Java_Main_unwindInProcess+865)\n"
      "  #01 pc 0000212d (offset 0x2000)  137-cfi.odex (boolean Main.unwindInProcess(boolean, int, "
      "boolean)+92)\n"
      "  #02 pc 00011cb1  anonymous:e2796000 (boolean Main.bar(boolean)+72)\n"
      "  #03 pc 00462175  libartd.so (art_quick_invoke_stub_internal+68)\n"
      "  #04 pc 00467129  libartd.so (art_quick_invoke_stub+228)\n"
      "  #05 pc 000bf7a9  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+864)\n"
      "  #06 pc 00247833  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+382)\n"
      "  #07 pc 0022e935  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+244)\n"
      "  #08 pc 0022f71d  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+128)\n"
      "  #09 pc 00442865  libartd.so (artQuickToInterpreterBridge+796)\n"
      "  #10 pc 004666ff  libartd.so (art_quick_to_interpreter_bridge+30)\n"
      "  #11 pc 00011c31  anonymous:e2796000 (int Main.compare(Main, Main)+64)\n"
      "  #12 pc 00462175  libartd.so (art_quick_invoke_stub_internal+68)\n"
      "  #13 pc 00467129  libartd.so (art_quick_invoke_stub+228)\n"
      "  #14 pc 000bf7a9  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+864)\n"
      "  #15 pc 00247833  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+382)\n"
      "  #16 pc 0022e935  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+244)\n"
      "  #17 pc 0022f71d  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+128)\n"
      "  #18 pc 00442865  libartd.so (artQuickToInterpreterBridge+796)\n"
      "  #19 pc 004666ff  libartd.so (art_quick_to_interpreter_bridge+30)\n"
      "  #20 pc 00011b77  anonymous:e2796000 (int Main.compare(java.lang.Object, "
      "java.lang.Object)+118)\n"
      "  #21 pc 00462175  libartd.so (art_quick_invoke_stub_internal+68)\n"
      "  #22 pc 00467129  libartd.so (art_quick_invoke_stub+228)\n"
      "  #23 pc 000bf7a9  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+864)\n"
      "  #24 pc 00247833  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+382)\n"
      "  #25 pc 0022e935  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+244)\n"
      "  #26 pc 0022f71d  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+128)\n"
      "  #27 pc 00442865  libartd.so (artQuickToInterpreterBridge+796)\n"
      "  #28 pc 004666ff  libartd.so (art_quick_to_interpreter_bridge+30)\n"
      "  #29 pc 00011a29  anonymous:e2796000 (int "
      "java.util.Arrays.binarySearch0(java.lang.Object[], int, int, java.lang.Object, "
      "java.util.Comparator)+304)\n"
      "  #30 pc 00462175  libartd.so (art_quick_invoke_stub_internal+68)\n"
      "  #31 pc 0046722f  libartd.so (art_quick_invoke_static_stub+226)\n"
      "  #32 pc 000bf7bb  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+882)\n"
      "  #33 pc 00247833  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+382)\n"
      "  #34 pc 0022e935  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+244)\n"
      "  #35 pc 0022f71d  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+128)\n"
      "  #36 pc 00442865  libartd.so (artQuickToInterpreterBridge+796)\n"
      "  #37 pc 004666ff  libartd.so (art_quick_to_interpreter_bridge+30)\n"
      "  #38 pc 0001139b  anonymous:e2796000 (boolean Main.foo()+178)\n"
      "  #39 pc 00462175  libartd.so (art_quick_invoke_stub_internal+68)\n"
      "  #40 pc 00467129  libartd.so (art_quick_invoke_stub+228)\n"
      "  #41 pc 000bf7a9  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+864)\n"
      "  #42 pc 00247833  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+382)\n"
      "  #43 pc 0022e935  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+244)\n"
      "  #44 pc 0022f71d  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+128)\n"
      "  #45 pc 00442865  libartd.so (artQuickToInterpreterBridge+796)\n"
      "  #46 pc 004666ff  libartd.so (art_quick_to_interpreter_bridge+30)\n"
      "  #47 pc 00010aa7  anonymous:e2796000 (void Main.runPrimary()+70)\n"
      "  #48 pc 00462175  libartd.so (art_quick_invoke_stub_internal+68)\n"
      "  #49 pc 00467129  libartd.so (art_quick_invoke_stub+228)\n"
      "  #50 pc 000bf7a9  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+864)\n"
      "  #51 pc 00247833  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+382)\n"
      "  #52 pc 0022e935  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+244)\n"
      "  #53 pc 0022f71d  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+128)\n"
      "  #54 pc 00442865  libartd.so (artQuickToInterpreterBridge+796)\n"
      "  #55 pc 004666ff  libartd.so (art_quick_to_interpreter_bridge+30)\n"
      "  #56 pc 0000ba99  anonymous:e2796000 (void Main.main(java.lang.String[])+144)\n"
      "  #57 pc 00462175  libartd.so (art_quick_invoke_stub_internal+68)\n"
      "  #58 pc 0046722f  libartd.so (art_quick_invoke_static_stub+226)\n"
      "  #59 pc 000bf7bb  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+882)\n"
      "  #60 pc 00247833  libartd.so "
      "(_ZN3art11interpreter34ArtInterpreterToCompiledCodeBridgeEPNS_6ThreadEPNS_9ArtMethodEPNS_"
      "11ShadowFrameEtPNS_6JValueE+382)\n"
      "  #61 pc 0022e935  libartd.so "
      "(_ZN3art11interpreterL7ExecuteEPNS_6ThreadERKNS_20CodeItemDataAccessorERNS_11ShadowFrameENS_"
      "6JValueEb+244)\n"
      "  #62 pc 0022f71d  libartd.so "
      "(_ZN3art11interpreter30EnterInterpreterFromEntryPointEPNS_6ThreadERKNS_"
      "20CodeItemDataAccessorEPNS_11ShadowFrameE+128)\n"
      "  #63 pc 00442865  libartd.so (artQuickToInterpreterBridge+796)\n"
      "  #64 pc 004666ff  libartd.so (art_quick_to_interpreter_bridge+30)\n"
      "  #65 pc 00462175  libartd.so (art_quick_invoke_stub_internal+68)\n"
      "  #66 pc 0046722f  libartd.so (art_quick_invoke_static_stub+226)\n"
      "  #67 pc 000bf7bb  libartd.so "
      "(_ZN3art9ArtMethod6InvokeEPNS_6ThreadEPjjPNS_6JValueEPKc+882)\n"
      "  #68 pc 003b292d  libartd.so "
      "(_ZN3artL18InvokeWithArgArrayERKNS_33ScopedObjectAccessAlreadyRunnableEPNS_9ArtMethodEPNS_"
      "8ArgArrayEPNS_6JValueEPKc+52)\n"
      "  #69 pc 003b26c3  libartd.so "
      "(_ZN3art17InvokeWithVarArgsERKNS_33ScopedObjectAccessAlreadyRunnableEP8_jobjectP10_"
      "jmethodIDSt9__va_list+210)\n"
      "  #70 pc 00308411  libartd.so "
      "(_ZN3art3JNI21CallStaticVoidMethodVEP7_JNIEnvP7_jclassP10_jmethodIDSt9__va_list+76)\n"
      "  #71 pc 000e6a9f  libartd.so "
      "(_ZN3art8CheckJNI11CallMethodVEPKcP7_JNIEnvP8_jobjectP7_jclassP10_jmethodIDSt9__va_listNS_"
      "9Primitive4TypeENS_10InvokeTypeE+1486)\n"
      "  #72 pc 000e19b9  libartd.so "
      "(_ZN3art8CheckJNI21CallStaticVoidMethodVEP7_JNIEnvP7_jclassP10_jmethodIDSt9__va_list+40)\n"
      "  #73 pc 0000159f  dalvikvm32 "
      "(_ZN7_JNIEnv20CallStaticVoidMethodEP7_jclassP10_jmethodIDz+30)\n"
      "  #74 pc 00001349  dalvikvm32 (main+896)\n"
      "  #75 pc 000850c9  libc.so\n",
      frame_info);
}

// The eh_frame_hdr data is present but set to zero fdes. This should
// fallback to iterating over the cies/fdes and ignore the eh_frame_hdr.
// No .gnu_debugdata section in the elf file, so no symbols.
TEST_F(UnwindOfflineTest, bad_eh_frame_hdr_arm64) {
  Init("bad_eh_frame_hdr_arm64/", ARCH_ARM64);

  Unwinder unwinder(128, maps_.get(), regs_.get(), process_memory_);
  unwinder.Unwind();

  std::string frame_info(DumpFrames(unwinder));
  ASSERT_EQ(5U, unwinder.NumFrames()) << "Unwind:\n" << frame_info;
  EXPECT_EQ(
      "  #00 pc 0000000000000550  waiter64\n"
      "  #01 pc 0000000000000568  waiter64\n"
      "  #02 pc 000000000000057c  waiter64\n"
      "  #03 pc 0000000000000590  waiter64\n"
      "  #04 pc 00000000000a8e98  libc.so (__libc_init+88)\n",
      frame_info);
}

// The elf has bad eh_frame unwind information for the pcs. If eh_frame
// is used first, the unwind will not match the expected output.
TEST_F(UnwindOfflineTest, debug_frame_first_x86) {
  Init("debug_frame_first_x86/", ARCH_X86);

  Unwinder unwinder(128, maps_.get(), regs_.get(), process_memory_);
  unwinder.Unwind();

  std::string frame_info(DumpFrames(unwinder));
  ASSERT_EQ(5U, unwinder.NumFrames()) << "Unwind:\n" << frame_info;
  EXPECT_EQ(
      "  #00 pc 00000685  waiter (call_level3+53)\n"
      "  #01 pc 000006b7  waiter (call_level2+23)\n"
      "  #02 pc 000006d7  waiter (call_level1+23)\n"
      "  #03 pc 000006f7  waiter (main+23)\n"
      "  #04 pc 00018275  libc.so\n",
      frame_info);
}

// Make sure that a pc that is at the beginning of an fde unwinds correctly.
TEST_F(UnwindOfflineTest, eh_frame_hdr_begin_x86_64) {
  Init("eh_frame_hdr_begin_x86_64/", ARCH_X86_64);

  Unwinder unwinder(128, maps_.get(), regs_.get(), process_memory_);
  unwinder.Unwind();

  std::string frame_info(DumpFrames(unwinder));
  ASSERT_EQ(5U, unwinder.NumFrames()) << "Unwind:\n" << frame_info;
  EXPECT_EQ(
      "  #00 pc 0000000000000a80  unwind_test64 (calling3)\n"
      "  #01 pc 0000000000000dd9  unwind_test64 (calling2+633)\n"
      "  #02 pc 000000000000121e  unwind_test64 (calling1+638)\n"
      "  #03 pc 00000000000013ed  unwind_test64 (main+13)\n"
      "  #04 pc 00000000000202b0  libc.so\n",
      frame_info);
}

}  // namespace unwindstack