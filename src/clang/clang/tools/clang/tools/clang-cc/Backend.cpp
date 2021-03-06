//===--- Backend.cpp - Interface to LLVM backend technologies -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ASTConsumers.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/Frontend/CompileOptions.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclGroup.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/PassManager.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/SchedulerRegistry.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Timer.h"
#include "llvm/System/Path.h"
#include "llvm/System/Program.h"
#include "llvm/Target/SubtargetFeature.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetMachineRegistry.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/IPO.h"
using namespace clang;
using namespace llvm;

namespace {
  class VISIBILITY_HIDDEN BackendConsumer : public ASTConsumer {
    BackendAction Action;
    CompileOptions CompileOpts;
    const std::string &InputFile;
    std::string OutputFile;
    ASTContext *Context;

    Timer LLVMIRGeneration;
    Timer CodeGenerationTime;
    
    llvm::OwningPtr<CodeGenerator> Gen;
    
    llvm::Module *TheModule;
    llvm::TargetData *TheTargetData;
    llvm::raw_ostream *AsmOutStream;

    mutable llvm::ModuleProvider *ModuleProvider;
    mutable FunctionPassManager *CodeGenPasses;
    mutable PassManager *PerModulePasses;
    mutable FunctionPassManager *PerFunctionPasses;

    FunctionPassManager *getCodeGenPasses() const;
    PassManager *getPerModulePasses() const;
    FunctionPassManager *getPerFunctionPasses() const;

    void CreatePasses();

    /// AddEmitPasses - Add passes necessary to emit assembly or LLVM
    /// IR.
    ///
    /// \return True on success. On failure \arg Error will be set to
    /// a user readable error message.
    bool AddEmitPasses(std::string &Error);

    void EmitAssembly();
    
  public:  
    BackendConsumer(BackendAction action, Diagnostic &Diags, 
                    const LangOptions &langopts, const CompileOptions &compopts,
                    const std::string &infile, const std::string &outfile) :
      Action(action), 
      CompileOpts(compopts),
      InputFile(infile), 
      OutputFile(outfile), 
      LLVMIRGeneration("LLVM IR Generation Time"),
      CodeGenerationTime("Code Generation Time"),
      Gen(CreateLLVMCodeGen(Diags, InputFile, compopts)),
      TheModule(0), TheTargetData(0), AsmOutStream(0), ModuleProvider(0),
      CodeGenPasses(0), PerModulePasses(0), PerFunctionPasses(0) {
      
      // Enable -time-passes if -ftime-report is enabled.
      llvm::TimePassesIsEnabled = CompileOpts.TimePasses;
    }

    ~BackendConsumer() {
      delete AsmOutStream;
      delete TheTargetData;
      delete ModuleProvider;
      delete CodeGenPasses;
      delete PerModulePasses;
      delete PerFunctionPasses;
    }

    virtual void Initialize(ASTContext &Ctx) {
      Context = &Ctx;
      
      if (CompileOpts.TimePasses)
        LLVMIRGeneration.startTimer();
      
      Gen->Initialize(Ctx);

      TheModule = Gen->GetModule();
      ModuleProvider = new ExistingModuleProvider(TheModule);
      TheTargetData = new llvm::TargetData(Ctx.Target.getTargetDescription());
      
      if (CompileOpts.TimePasses)
        LLVMIRGeneration.stopTimer();
    }
    
    virtual void HandleTopLevelDecl(DeclGroupRef D) {
      PrettyStackTraceDecl CrashInfo(*D.begin(), SourceLocation(),
                                     Context->getSourceManager(),
                                     "LLVM IR generation of declaration");
      
      if (CompileOpts.TimePasses)
        LLVMIRGeneration.startTimer();

      Gen->HandleTopLevelDecl(D);

      if (CompileOpts.TimePasses)
        LLVMIRGeneration.stopTimer();
    }
    
    virtual void HandleTranslationUnit(ASTContext &C) {
      {
        PrettyStackTraceString CrashInfo("Per-file LLVM IR generation");
        if (CompileOpts.TimePasses)
          LLVMIRGeneration.startTimer();

        Gen->HandleTranslationUnit(C);

        if (CompileOpts.TimePasses)
          LLVMIRGeneration.stopTimer();
      }

      // EmitAssembly times and registers crash info itself.
      EmitAssembly();
      
      // Force a flush here in case we never get released.
      if (AsmOutStream)
        AsmOutStream->flush();
    }
    
    virtual void HandleTagDeclDefinition(TagDecl *D) {
      PrettyStackTraceDecl CrashInfo(D, SourceLocation(),
                                     Context->getSourceManager(),
                                     "LLVM IR generation of declaration");
      Gen->HandleTagDeclDefinition(D);
    }

    virtual void CompleteTentativeDefinition(VarDecl *D) {
      Gen->CompleteTentativeDefinition(D);
    }
  };  
}

FunctionPassManager *BackendConsumer::getCodeGenPasses() const {
  if (!CodeGenPasses) {
    CodeGenPasses = new FunctionPassManager(ModuleProvider);
    CodeGenPasses->add(new TargetData(*TheTargetData));
  }

  return CodeGenPasses;
}

PassManager *BackendConsumer::getPerModulePasses() const {
  if (!PerModulePasses) {
    PerModulePasses = new PassManager();
    PerModulePasses->add(new TargetData(*TheTargetData));
  }

  return PerModulePasses;
}

FunctionPassManager *BackendConsumer::getPerFunctionPasses() const {
  if (!PerFunctionPasses) {
    PerFunctionPasses = new FunctionPassManager(ModuleProvider);
    PerFunctionPasses->add(new TargetData(*TheTargetData));
  }

  return PerFunctionPasses;
}

bool BackendConsumer::AddEmitPasses(std::string &Error) {
  if (Action == Backend_EmitNothing)
    return true;

  if (OutputFile == "-" || (InputFile == "-" && OutputFile.empty())) {
    AsmOutStream = new raw_stdout_ostream();
    sys::Program::ChangeStdoutToBinary();
  } else {
    if (OutputFile.empty()) {
      llvm::sys::Path Path(InputFile);
      Path.eraseSuffix();
      if (Action == Backend_EmitBC) {
        Path.appendSuffix("bc");
      } else if (Action == Backend_EmitLL) {
        Path.appendSuffix("ll");
      } else {
        Path.appendSuffix("s");
      }
      OutputFile = Path.toString();
    }

    AsmOutStream = new raw_fd_ostream(OutputFile.c_str(), true, Error);
    if (!Error.empty())
      return false;
  }

  if (Action == Backend_EmitBC) {
    getPerModulePasses()->add(createBitcodeWriterPass(*AsmOutStream));
  } else if (Action == Backend_EmitLL) {
    getPerModulePasses()->add(createPrintModulePass(AsmOutStream));
  } else {
    bool Fast = CompileOpts.OptimizationLevel == 0;

    // Create the TargetMachine for generating code.
    const TargetMachineRegistry::entry *TME = 
      TargetMachineRegistry::getClosestStaticTargetForModule(*TheModule, Error);
    if (!TME) {
      Error = std::string("Unable to get target machine: ") + Error;
      return false;
    }

    std::string FeaturesStr;
    if (CompileOpts.CPU.size() || CompileOpts.Features.size()) {
      SubtargetFeatures Features;
      Features.setCPU(CompileOpts.CPU);
      for (std::vector<std::string>::iterator 
             it = CompileOpts.Features.begin(),
             ie = CompileOpts.Features.end(); it != ie; ++it)
        Features.AddFeature(*it);
      FeaturesStr = Features.getString();
    }
    TargetMachine *TM = TME->CtorFn(*TheModule, FeaturesStr);
    
    // Set register scheduler & allocation policy.
    RegisterScheduler::setDefault(createDefaultScheduler);
    RegisterRegAlloc::setDefault(Fast ? createLocalRegisterAllocator : 
                                 createLinearScanRegisterAllocator);  

    // From llvm-gcc:
    // If there are passes we have to run on the entire module, we do codegen
    // as a separate "pass" after that happens.
    // FIXME: This is disabled right now until bugs can be worked out.  Reenable
    // this for fast -O0 compiles!
    FunctionPassManager *PM = getCodeGenPasses();
    CodeGenOpt::Level OptLevel = CodeGenOpt::Default;

    switch (CompileOpts.OptimizationLevel) {
    default: break;
    case 0: OptLevel = CodeGenOpt::None; break;
    case 3: OptLevel = CodeGenOpt::Aggressive; break;
    }

    // Normal mode, emit a .s file by running the code generator.
    // Note, this also adds codegenerator level optimization passes.
    switch (TM->addPassesToEmitFile(*PM, *AsmOutStream,
                                    TargetMachine::AssemblyFile, OptLevel)) {
    default:
    case FileModel::Error:
      Error = "Unable to interface with target machine!\n";
      return false;
    case FileModel::AsmFile:
      break;
    }
    
    if (TM->addPassesToEmitFileFinish(*CodeGenPasses, 0, OptLevel)) {
      Error = "Unable to interface with target machine!\n";
      return false;
    }
  }

  return true;
}

void BackendConsumer::CreatePasses() {
  // In -O0 if checking is disabled, we don't even have per-function passes.
  if (CompileOpts.VerifyModule)
    getPerFunctionPasses()->add(createVerifierPass());

  if (CompileOpts.OptimizationLevel > 0) {
    FunctionPassManager *PM = getPerFunctionPasses();
    PM->add(createCFGSimplificationPass());
    if (CompileOpts.OptimizationLevel == 1)
      PM->add(createPromoteMemoryToRegisterPass());
    else
      PM->add(createScalarReplAggregatesPass());
    PM->add(createInstructionCombiningPass());
  }

  // For now we always create per module passes.
  PassManager *PM = getPerModulePasses();
  if (CompileOpts.OptimizationLevel > 0) {
    if (CompileOpts.UnitAtATime)
      PM->add(createRaiseAllocationsPass());      // call %malloc -> malloc inst
    PM->add(createCFGSimplificationPass());       // Clean up disgusting code
    PM->add(createPromoteMemoryToRegisterPass()); // Kill useless allocas
    if (CompileOpts.UnitAtATime) {
      PM->add(createGlobalOptimizerPass());       // Optimize out global vars
      PM->add(createGlobalDCEPass());             // Remove unused fns and globs
      PM->add(createIPConstantPropagationPass()); // IP Constant Propagation
      PM->add(createDeadArgEliminationPass());    // Dead argument elimination
    }
    PM->add(createInstructionCombiningPass());    // Clean up after IPCP & DAE
    PM->add(createCFGSimplificationPass());       // Clean up after IPCP & DAE
    if (CompileOpts.UnitAtATime) {
      PM->add(createPruneEHPass());               // Remove dead EH info
      PM->add(createFunctionAttrsPass());         // Set readonly/readnone attrs
    }
    if (CompileOpts.InlineFunctions) {
      // Inline small functions
      unsigned Threshold = (CompileOpts.OptimizeSize ||
                            CompileOpts.OptimizationLevel < 3) ? 50 : 200;
      PM->add(createFunctionInliningPass(Threshold));
    } else 
      PM->add(createAlwaysInlinerPass());         // Respect always_inline
    if (CompileOpts.OptimizationLevel > 2)
      PM->add(createArgumentPromotionPass());     // Scalarize uninlined fn args
    if (CompileOpts.SimplifyLibCalls)
      PM->add(createSimplifyLibCallsPass());      // Library Call Optimizations
    PM->add(createInstructionCombiningPass());    // Cleanup for scalarrepl.
    PM->add(createJumpThreadingPass());           // Thread jumps.
    PM->add(createCFGSimplificationPass());       // Merge & remove BBs
    PM->add(createScalarReplAggregatesPass());    // Break up aggregate allocas
    PM->add(createInstructionCombiningPass());    // Combine silly seq's
    PM->add(createCondPropagationPass());         // Propagate conditionals
    PM->add(createTailCallEliminationPass());     // Eliminate tail calls
    PM->add(createCFGSimplificationPass());       // Merge & remove BBs
    PM->add(createReassociatePass());             // Reassociate expressions
    PM->add(createLoopRotatePass());              // Rotate Loop
    PM->add(createLICMPass());                    // Hoist loop invariants
    PM->add(createLoopUnswitchPass(CompileOpts.OptimizeSize ? true : false));
//    PM->add(createLoopIndexSplitPass());          // Split loop index
    PM->add(createInstructionCombiningPass());  
    PM->add(createIndVarSimplifyPass());          // Canonicalize indvars
    PM->add(createLoopDeletionPass());            // Delete dead loops
    if (CompileOpts.UnrollLoops)
      PM->add(createLoopUnrollPass());            // Unroll small loops
    PM->add(createInstructionCombiningPass());    // Clean up after the unroller
    PM->add(createGVNPass());                     // Remove redundancies
    PM->add(createMemCpyOptPass());               // Remove memcpy / form memset
    PM->add(createSCCPPass());                    // Constant prop with SCCP
    
    // Run instcombine after redundancy elimination to exploit opportunities
    // opened up by them.
    PM->add(createInstructionCombiningPass());
    PM->add(createCondPropagationPass());         // Propagate conditionals
    PM->add(createDeadStoreEliminationPass());    // Delete dead stores
    PM->add(createAggressiveDCEPass());           // Delete dead instructions
    PM->add(createCFGSimplificationPass());       // Merge & remove BBs

    if (CompileOpts.UnitAtATime) {
      PM->add(createStripDeadPrototypesPass());   // Get rid of dead prototypes
      PM->add(createDeadTypeEliminationPass());   // Eliminate dead types
    }

    if (CompileOpts.OptimizationLevel > 1 && CompileOpts.UnitAtATime)
      PM->add(createConstantMergePass());         // Merge dup global constants 
  } else {
    PM->add(createAlwaysInlinerPass());  
  }
}

/// EmitAssembly - Handle interaction with LLVM backend to generate
/// actual machine code. 
void BackendConsumer::EmitAssembly() {
  // Silently ignore if we weren't initialized for some reason.
  if (!TheModule || !TheTargetData)
    return;
  
  
  TimeRegion Region(CompileOpts.TimePasses ? &CodeGenerationTime : 0);

  // Make sure IR generation is happy with the module. This is
  // released by the module provider.
  Module *M = Gen->ReleaseModule();
  if (!M) {
    // The module has been released by IR gen on failures, do not
    // double free.
    ModuleProvider->releaseModule();
    TheModule = 0;
    return;
  }

  assert(TheModule == M && "Unexpected module change during IR generation");

  CreatePasses();

  std::string Error;
  if (!AddEmitPasses(Error)) {
    // FIXME: Don't fail this way.
    llvm::cerr << "ERROR: " << Error << "\n";
    ::exit(1);
  }

  // Run passes. For now we do all passes at once, but eventually we
  // would like to have the option of streaming code generation.

  if (PerFunctionPasses) {
    PrettyStackTraceString CrashInfo("Per-function optimization");
    
    PerFunctionPasses->doInitialization();
    for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I)
      if (!I->isDeclaration())
        PerFunctionPasses->run(*I);
    PerFunctionPasses->doFinalization();
  }
  
  if (PerModulePasses) {
    PrettyStackTraceString CrashInfo("Per-module optimization passes");
    PerModulePasses->run(*M);
  }
  
  if (CodeGenPasses) {
    PrettyStackTraceString CrashInfo("Code generation");
    CodeGenPasses->doInitialization();
    for (Module::iterator I = M->begin(), E = M->end(); I != E; ++I)
      if (!I->isDeclaration())
        CodeGenPasses->run(*I);
    CodeGenPasses->doFinalization();
  }
}

ASTConsumer *clang::CreateBackendConsumer(BackendAction Action,
                                          Diagnostic &Diags,
                                          const LangOptions &LangOpts,
                                          const CompileOptions &CompileOpts,
                                          const std::string& InFile,
                                          const std::string& OutFile) {
  return new BackendConsumer(Action, Diags, LangOpts, CompileOpts,
                             InFile, OutFile);  
}
