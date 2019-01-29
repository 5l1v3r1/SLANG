//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Ronak Chauhan (r.chauhan@somaiya.edu)
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//AD If SlangGenChecker class name is added or changed, then also edit,
//AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//
//===----------------------------------------------------------------------===//
//

#include "ClangSACheckers.h"
#include "clang/AST/Decl.h" //AD
#include "clang/AST/Expr.h" //AD
#include "clang/AST/Stmt.h" //AD
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/Analyses/LiveVariables.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/AnalysisManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExplodedGraph.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h" //AD
#include <string>                     //AD
#include <vector>                     //AD
#include <utility>                    //AD
#include <unordered_map>              //AD
#include <fstream>                    //AD
#include <sstream>                    //AD
#include <stack>                      //AD

using namespace clang;
using namespace ento;

// non-breaking space
#define NBSP2  "  "
#define NBSP4  "    "
#define NBSP6  "      "
#define NBSP8  "        "
#define NBSP10 "          "

// #define LOG_ME(X) if (Utility::debug_mode) Utility::log((X), __FUNCTION__, __LINE__)

//===----------------------------------------------------------------------===//
// FIXME: Utility Class
// Some useful static utility functions in a class.
//===----------------------------------------------------------------------===//
namespace {
    class Utility {
    public:
        static void readFile1();
        static bool debug_mode;
        static bool LS; // Log Switch
        static void log(std::string msg, std::string func_name, uint32_t line);
    };
}

bool Utility::debug_mode = false;
bool Utility::LS = false;

void Utility::log(std::string msg, std::string func_name, uint32_t line) {
    llvm::errs() << "SLANG: " << func_name << ":" << line << " " << msg << "\n";
}

void Utility::readFile1() {
    //Read a specific local file.
    std::ifstream inputTxtFile;
    std::string line;
    std::string fileName("/home/codeman/.itsoflife/local/tmp/checker-input.txt");
    inputTxtFile.open(fileName);
    if (inputTxtFile.is_open()) {
      while(std::getline(inputTxtFile, line)) {
          llvm::errs() << line << "\n";
      }
      inputTxtFile.close();
    } else {
        llvm::errs() << "SLANG: ERROR: Cannot open file '" << fileName << "'\n";
    }
}

//===----------------------------------------------------------------------===//
// FIXME: Test TraversedInfoBuffer functionality.
// Information is stored while traversing the function's CFG. Specifically,
// 1. The information of the variables used.
// 2. The CFG and the basic block structure.
// 3. The three address code representation.
//===----------------------------------------------------------------------===//
namespace {
    enum EdgeLabel {FalseEdge = 0, TrueEdge = 1, UnCondEdge = 2};

    class VarInfo {
    public:
        uint64_t id;
        // variable name: e.g. a variable 'x' in main function, is "v:main:x".
        std::string var_name;
        std::string type_str;

        std::string convertToString() {
            std::stringstream ss;
            ss << "\"" << var_name << "\": " << type_str << ",";
            return ss.str();
        }
    };

    class TraversedInfoBuffer {
    public:
        int id;
        uint32_t tmp_var_counter;
        uint32_t curr_bb_id;

        std::string func_name;
        std::string func_ret_t;
        std::string func_params;

        // stack to help convert ast structure to 3-address code.
        std::stack<const Stmt*> main_stack;
        // contains names of the temporary vars for expressions.
        std::stack<std::string> sub_stack;

        // maps a unique variable id to its string representation.
        std::unordered_map<uint64_t , VarInfo> var_map;
        // maps bb_edge
        std::vector<std::pair<int32_t, std::pair<int32_t , EdgeLabel>>> bb_edges;
        // entry bb id is mapped to -1
        std::unordered_map<uint32_t , std::vector<std::string>> bb_stmts;

        std::vector<std::string> edge_labels;

        TraversedInfoBuffer();
        void clear();

        std::string genTmpVariable(QualType qt);
        uint32_t nextTmpCount();

        // conversion_routines 1 to SPAN Strings
        std::string convertClangType(QualType qt);
        std::string convertFuncName(std::string func_name);
        std::string convertVarExpr(uint64_t var_addr);
        std::string convertLocalVarName(std::string var_name);
        std::string convertGlobalVarName(std::string var_name);
        std::string convertBbStmts(const std::vector<std::string>& stmts);
        std::string convertBbEdges();

        // SPAN IR dumping_routines
        void dumpSpanIr();
        void dumpHeader();
        void dumpVariables();
        void dumpFunctions();
        void dumpFooter();

        // // For Program state purpose.
        // // Overload the == operator
        // bool operator==(const TraversedInfoBuffer &tib) const;
        // // LLVMs equivalent of a hash function
        // void Profile(llvm::FoldingSetNodeID &ID) const;
    };
}

TraversedInfoBuffer::TraversedInfoBuffer(): id{1}, tmp_var_counter{}, main_stack{}, sub_stack{},
    var_map{}, bb_edges{}, bb_stmts{}, edge_labels{3} {

    edge_labels[FalseEdge] = "FalseEdge";
    edge_labels[TrueEdge] = "TrueEdge";
    edge_labels[UnCondEdge] = "UnCondEdge";
}

uint32_t TraversedInfoBuffer::nextTmpCount() {
    tmp_var_counter += 1;
    return tmp_var_counter;
}

std::string TraversedInfoBuffer::convertFuncName(std::string func_name) {
    std::stringstream ss;
    ss << "f:" << func_name;
    return ss.str();
}

std::string TraversedInfoBuffer::convertGlobalVarName(std::string var_name) {
    std::stringstream ss;
    ss << "v:" << var_name;
    return ss.str();
}

std::string TraversedInfoBuffer::convertLocalVarName(std::string var_name) {
    // assumes func_name is set
    std::stringstream ss;
    ss << "v:" << func_name << ":" << var_name;
    return ss.str();
}

std::string TraversedInfoBuffer::genTmpVariable(const QualType qt) {
    std::stringstream ss;
    // STEP 1: generate the name.
    uint32_t var_id = nextTmpCount();
    ss << "v:" << func_name << ":t." << var_id;

    // STEP 2: register the tmp var and its type.
    VarInfo varInfo{};
    varInfo.var_name = ss.str();
    varInfo.type_str = convertClangType(qt);

    // STEP 3: Add to the var map.
    // The 'var_id' here should be small enough to interfere with uint64_t addresses.
    var_map[var_id] = varInfo;

    return varInfo.var_name;
}

// Converts Clang Types into SPAN parsable type strings:
// Examples,
// for `int` it returns `types.Int`.
// for `int*` it returns `types.Ptr(to=types.Int)`.
std::string TraversedInfoBuffer::convertClangType(QualType qt) {
    std::stringstream ss;
    const Type *type = qt.getTypePtr();
    if (type->isBuiltinType()) {
        if(type->isIntegerType()) {
            if(type->isCharType()) {
                ss << "types.Char";
            } else {
                ss << "types.Int";
            }
        } else if(type->isFloatingType()) {
            ss << "types.Float";
        } else if(type->isVoidType()) {
            ss << "types.Void";
        } else {
            ss << "UnknownBuiltinType.";
        }
    } else if(type->isPointerType()) {
        ss << "types.Ptr(to=";
        QualType pqt = type->getPointeeType();
        ss << convertClangType(pqt);
        ss << ")";
    } else {
        ss << "UnknownType.";
    }

    return ss.str();
} // convertClangType()

//BOUND START: conversion_routines 1

std::string TraversedInfoBuffer::convertVarExpr(uint64_t var_addr) {
    // if here, var should already be in var_map
    std::stringstream ss;

    auto vinfo = var_map[var_addr];
    ss << vinfo.var_name;

    return ss.str();
}

std::string TraversedInfoBuffer::convertBbStmts(const std::vector<std::string>& stmts) {
    std::stringstream ss;

    for (auto stmt : stmts) {
        ss << stmt << ",\n";
    }

    return ss.str();
} // convertBbStmts()

//BOUND END  : conversion_routines 1

// clear the buffer for the next function.
void TraversedInfoBuffer::clear() {
    func_name = "";
    func_ret_t = "";
    func_params = "";
    curr_bb_id = 0;
    tmp_var_counter = 0;

    var_map.clear();
    bb_edges.clear();
    bb_stmts.clear();
    while (!main_stack.empty()) {
        main_stack.pop();
    }
    while (!sub_stack.empty()) {
        sub_stack.pop();
    }
}

//BLOCK START: dumping_routines

void TraversedInfoBuffer::dumpVariables() {
    llvm::errs() << "all_vars: Dict[types.VarNameT, types.ReturnT] = {\n";
    for (auto var: var_map) {
        // with indent of two spaces
        llvm::errs() << "  ";
        llvm::errs() << "\"" << var.second.var_name
                     << "\": " << var.second.type_str << ",\n";
    }
    llvm::errs() << "} # end all_vars dict\n\n";
}

void TraversedInfoBuffer::dumpHeader() {
    std::stringstream ss;

    ss << "#!/usr/bin/env python3\n";
    ss << "\n";
    ss << "# MIT License.\n";
    ss << "# Copyright (c) 2019 The SLANG Authors.\n";
    ss << "\n";
    ss << "\"\"\"\n";
    ss << "Slang (SPAN IR) program.\n";
    ss << "\"\"\"\n";
    ss << "\n";
    ss << "from typing import Dict\n";
    ss << "\n";
    ss << "import span.ir.types as types\n";
    ss << "import span.ir.expr as expr\n";
    ss << "import span.ir.instr as instr\n";
    ss << "\n";
    ss << "import span.sys.graph as graph\n";
    ss << "import span.sys.universe as universe\n";
    ss << "\n";
    ss << "# analysis unit name\n";
    ss << "name = \"SLANG\"\n";
    ss << "description = \"Auto-Translated from Clang AST.\"\n";
    ss << "\n";

    llvm::errs() << ss.str();
}

void TraversedInfoBuffer::dumpFooter() {
    std::stringstream ss;
    ss << "\n";
    ss << "# Always build the universe from a 'program' module.\n";
    ss << "# Initialize the universe with program in this module.\n";
    ss << "universe.build(name, description, all_vars, all_func)\n";
    llvm::errs() << ss.str();
}

void TraversedInfoBuffer::dumpFunctions() {
    llvm::errs() << "all_func: Dict[types.FuncNameT, graph.FuncNode] = {\n";

    llvm::errs() << NBSP2; // indent
    llvm::errs() << "\"" << convertFuncName(func_name) << "\":\n";
    llvm::errs() << NBSP4 << "graph.FuncNode(\n";

    // fields
    llvm::errs() << NBSP6 << "name= " << "\"" << convertFuncName(func_name) << "\",\n";
    llvm::errs() << NBSP6 << "params= [" << func_params << "],\n";
    llvm::errs() << NBSP6 << "returns= " << func_ret_t << ",\n";

    // fields: basic_blocks
    llvm::errs() << "\n";
    llvm::errs() << NBSP6 << "# if -1, its start_block. (REQUIRED)\n";
    llvm::errs() << NBSP6 << "basic_blocks= {\n";
    for (auto bb : bb_stmts) {
        llvm::errs() << NBSP8 << bb.first << ": graph.BB([\n";
        for(auto stmt: bb.second) {
            llvm::errs() << NBSP10 << stmt << ",\n";
        }
    }
    llvm::errs() << NBSP8 << "]),\n";

    // fields: bb_edges
    llvm::errs() << "\n";
    llvm::errs() << NBSP6 << "bb_edges= [\n";
    llvm::errs() << convertBbEdges();
    llvm::errs() << NBSP6 << "],\n";

    // close this function data structure
    llvm::errs() << NBSP4 << "), # " << convertFuncName(func_name) << "() end. \n\n";

    // close all_func
    llvm::errs() << "} # end all_func dict.\n";
}

// dump entire span ir for the translation unit.
void TraversedInfoBuffer::dumpSpanIr() {
    dumpHeader();
    dumpVariables();
    dumpFunctions();
    dumpFooter();
} // dumpSpanIr()

//BLOCK END  : dumping_routines

std::string TraversedInfoBuffer::convertBbEdges() {
    std::stringstream ss;

    for (auto p: bb_edges) {
        ss << NBSP8 << "graph.BbEdge(" << std::to_string(p.first);
        ss << ", " << std::to_string(p.second.first) << ", ";
        ss << "graph." << edge_labels[p.second.second] << "),\n";
    }

    return ss.str();
}

// // For Program state's purpose: not in use currently.
// // Overload the == operator
// bool TraversedInfoBuffer::operator==(const TraversedInfoBuffer &tib) const { return id == tib.id; }
// // LLVMs equivalent of a hash function
// void TraversedInfoBuffer::Profile(llvm::FoldingSetNodeID &ID) const { ID.AddInteger(1); }


//===----------------------------------------------------------------------===//
// SlangGenChecker
//===----------------------------------------------------------------------===//

namespace {
    class SlangGenChecker : public Checker<check::ASTCodeBody> {
        static TraversedInfoBuffer tib;

    public:
        void checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                              BugReporter &BR) const;

        // handling_routines
        void handleFunctionDef(const FunctionDecl *D) const;
        void handleCfg(const CFG *cfg) const;
        void handleBBInfo(const CFGBlock *bb, const CFG *cfg) const;
        void handleBBStmts(const CFGBlock *bb) const;
        void handleVariable(const ValueDecl *valueDecl) const;
        void handleDeclStmt(const DeclStmt *declStmt) const;
        void handleDeclRefExpr(const DeclRefExpr *DRE) const;
        void handleBinaryOperator(const BinaryOperator *binOp) const;

        // helper_functions
        void addStmtToCurrBlock(std::string stmt) const;

        // conversion_routines
        std::pair<std::string, bool> convertAssignment(const BinaryOperator *binOp) const;
        std::pair<std::string, bool> convertIntegerLiteral(const IntegerLiteral *IL) const;
        // a function, if stmt, *y on lhs, arr[i] on lhs are examples of a compound_receiver.
        std::pair<std::string, bool> convertExpr(bool compound_receiver) const;
        std::pair<std::string, bool> convertDeclRefExpr(const DeclRefExpr *dre) const;
        std::pair<std::string, bool> convertVarDecl(const VarDecl *varDecl) const;

    }; // class SlangGenChecker
} // anonymous namespace

TraversedInfoBuffer SlangGenChecker::tib = TraversedInfoBuffer();

// Main Entry Point. Invokes top level Function and Cfg handlers.
// Invoked once for each source translation unit function.
void SlangGenChecker::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                   BugReporter &BR) const {
    Utility::readFile1();
    llvm::errs() << "\nBOUND START: SLANG_Generated_Output.\n";

    tib.clear(); // clear the buffer for this function.

    const FunctionDecl *func_decl = dyn_cast<FunctionDecl>(D);
    handleFunctionDef(func_decl);

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
        tib.dumpSpanIr();
    } else {
        llvm::errs() << "SLANG: ERROR: No CFG for function.\n";
    }

    llvm::errs() << "\nBOUND END  : SLANG_Generated_Output.\n";
} // checkASTCodeBody()

//BOUND START: handling_routines

void SlangGenChecker::handleCfg(const CFG *cfg) const {
    for (const CFGBlock *bb : *cfg) {
        tib.curr_bb_id = bb->getBlockID();
        handleBBInfo(bb, cfg);
        handleBBStmts(bb);
    }
} // handleCfg()

// Gets the function name, paramaters and return type.
void SlangGenChecker::handleFunctionDef(const FunctionDecl *func_decl) const {
    // STEP 1.1: Get function name.
    tib.func_name = func_decl->getNameInfo().getAsString();

    // STEP 1.2: Get function parameters.
    std::stringstream ss;
    std::string prefix = "";
    if (func_decl->doesThisDeclarationHaveABody()) { //& !func_decl->hasPrototype())
        for (unsigned i = 0, e = func_decl->getNumParams(); i != e; ++i) {
            const ParmVarDecl *paramVarDecl = func_decl->getParamDecl(i);
            handleVariable(paramVarDecl);
            if (i != 0) {
                prefix = ", ";
            }
            ss << prefix << "\"" << tib.convertVarExpr((uint64_t)paramVarDecl) << "\"";
        }
        tib.func_params = ss.str();
    }

    // STEP 1.3: Get function return type.
    const QualType returnQType = func_decl->getReturnType();
    tib.func_ret_t = tib.convertClangType(returnQType);
} // handleFunctionDef()

void SlangGenChecker::handleBBInfo(const CFGBlock *bb, const CFG *cfg) const {
    int32_t succ_id, bb_id;
    unsigned entry_bb_id = cfg->getEntry().getBlockID();

    if ((bb_id = bb->getBlockID()) == (int32_t)entry_bb_id) {
        bb_id = -1; //entry block is ided -1.
    }

    llvm::errs() << "BB" << bb_id << "\n";

    if (bb == &cfg->getEntry()) {
        llvm::errs() << "ENTRY BB\n";
    } else if (bb == &cfg->getExit()) {
        llvm::errs() << "EXIT BB\n";
    }

    // access and record successor blocks
    const Stmt *terminator = (bb->getTerminator()).getStmt();
    if (terminator && isa<IfStmt>(terminator)) {
        bool true_edge = true;
        if (bb->succ_size() > 2) {
            llvm::errs() << "SPAN: ERROR: 'If' has more than two successors.\n";
        }

        for (CFGBlock::const_succ_iterator I = bb->succ_begin();
             I != bb->succ_end(); ++I) {

            CFGBlock *succ = *I;
            if ((succ_id = succ->getBlockID()) == (int32_t)entry_bb_id) {
                succ_id = -1;
            }

            if (true_edge) {
                tib.bb_edges.push_back(std::make_pair(bb_id, std::make_pair(succ_id, TrueEdge)));
                true_edge = false;
            } else {
                tib.bb_edges.push_back(std::make_pair(bb_id, std::make_pair(succ_id, FalseEdge)));
            }
        }
    } else {
        if (!bb->succ_empty()) {
            // num. of succ: bb->succ_size()
            for (CFGBlock::const_succ_iterator I = bb->succ_begin();
                    I != bb->succ_end(); ++I) {
                CFGBlock *succ = *I;
                if (!succ) {
                    // unreachable block ??
                    succ = I->getPossiblyUnreachableBlock();
                    llvm::errs() << "(Unreachable BB)";
                    continue;
                }

                if ((succ_id = succ->getBlockID()) == (int32_t)entry_bb_id) {
                    succ_id = -1;
                }

                tib.bb_edges.push_back(std::make_pair(bb_id, std::make_pair(succ_id, UnCondEdge)));
            }
        }
    }
} // handleBBInfo()

void SlangGenChecker::handleBBStmts(const CFGBlock *bb) const {
    std::unordered_map<const Expr *, int> visited_nodes;

    unsigned bb_id = bb->getBlockID();

    for (auto elem : *bb) {
        // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
        // ref for printing block:
        // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

        Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
        const Stmt *stmt = CS->getStmt();
        Stmt::StmtClass stmt_cls = stmt->getStmtClass();

        llvm::errs() << "Processing: " << stmt->getStmtClassName() << "\n";

        // the main statement selection conditions.
        // Ronak and Manav: Create a separate function,
        // to handle each kind of statement/expression.
        if (Utility::debug_mode) {
            llvm::errs() << "Unhandled " << stmt->getStmtClassName() << "\n";
            stmt->dump();
            llvm::errs() << "\n";
        } else {
            switch (stmt_cls) {
                default: {
                    // push to stack by default.
                    tib.main_stack.push(stmt);

                    llvm::errs() << "SLANG: DEFAULT: Pushed to stack: " <<
                                 stmt->getStmtClassName() << ".\n";
                    stmt->dump();
                    llvm::errs() << "\n";
                    break;
                }

                case Stmt::DeclRefExprClass: {
                    tib.main_stack.push(stmt);
                    const DeclRefExpr *declRefExpr = cast<DeclRefExpr>(stmt);
                    handleDeclRefExpr(declRefExpr);
                    break;
                }

                case Stmt::DeclStmtClass: {
                    const DeclStmt *declStmt = cast<DeclStmt>(stmt);
                    handleDeclStmt(declStmt);
                    break;
                }

                case Stmt::BinaryOperatorClass: {
                    const BinaryOperator *binOp = cast<BinaryOperator>(stmt);
                    handleBinaryOperator(binOp);
                    break;
                }

                case Stmt::ImplicitCastExprClass: {
                    // do nothing
                    break;
                }
            }
        }
    } // for (auto elem : *bb)

    // get terminator
    const Stmt *terminator = (bb->getTerminator()).getStmt();
    // handleTerminator(terminator, visited_nodes, bb_id);

    llvm::errs() << "\n\n";
} // handleBBStmts()

// record the variable name and type
void SlangGenChecker::handleVariable(const ValueDecl *valueDecl) const {
    uint64_t var_id = (uint64_t) valueDecl;
    if (tib.var_map.find(var_id) == tib.var_map.end()) {
        // seeing the variable for the first time.
        VarInfo varInfo{};
        varInfo.id = var_id;
        const VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl);
        if (varDecl) {
            if (varDecl->hasLocalStorage()) {
                varInfo.var_name = tib.convertLocalVarName(valueDecl->getNameAsString());
            } else if(varDecl->hasGlobalStorage()) {
                varInfo.var_name = tib.convertGlobalVarName(valueDecl->getNameAsString());
            } else if (varDecl->hasExternalStorage()) {
                llvm::errs() << "SLANG: ERROR: External Storage Not Handled.\n";
            } else {
                llvm::errs() << "SLANG: ERROR: Unknown variable storage.\n";
            }
        } else {
            llvm::errs() << "SLANG: ERROR: ValueDecl not a VarDecl!\n";
        }
        varInfo.type_str = tib.convertClangType(valueDecl->getType());
        tib.var_map[var_id] = varInfo;
        llvm::errs() << "NEW_VAR: " << varInfo.convertToString() << "\n";
    } else {
        llvm::errs() << "SEEN_VAR: " << tib.var_map[var_id].convertToString() << "\n";
    }
}

void SlangGenChecker::handleDeclStmt(const DeclStmt *declStmt) const {
    // assumes there is only single decl inside (the likely case).
    std::stringstream ss;

    const VarDecl *varDecl = cast<VarDecl>(declStmt->getSingleDecl());
    handleVariable(varDecl);

    if (!tib.main_stack.empty()) {
        // there is smth on the stack, hence on the rhs.
        auto pairLhs = convertVarDecl(varDecl);
        auto pairRhs = convertExpr(false);
        ss << "instr.AssignI(" << pairLhs.first << ", " << pairRhs.first << ")";
        addStmtToCurrBlock(ss.str());
    }
}

std::pair<std::string, bool> SlangGenChecker::convertIntegerLiteral(const IntegerLiteral *il) const {
    std::stringstream ss;

    bool is_signed = il->getType()->isSignedIntegerType();
    ss << "expr.Lit(" << il->getValue().toString(10, is_signed) << ")";
    llvm::errs() << ss.str() << "\n";

    return std::make_pair<std::string, bool>(ss.str(), false);
}

void SlangGenChecker::handleDeclRefExpr(const DeclRefExpr *declRefExpr) const {
    const ValueDecl *valueDecl = declRefExpr->getDecl();
    if (isa<VarDecl>(valueDecl)) {
        handleVariable(valueDecl);
    } else {
        llvm::errs() << "SLANG: ERROR: handleDeclRefExpr: unhandled "
                     << declRefExpr->getStmtClassName() << "\n";
    }
}

void SlangGenChecker::handleBinaryOperator(const BinaryOperator *binOp) const {
    if (binOp->isAssignmentOp()) {
        auto pair = convertAssignment(binOp);
        addStmtToCurrBlock(pair.first);
    }
}

std::pair<std::string, bool> SlangGenChecker::convertAssignment(const BinaryOperator *binOp) const {
    std::stringstream ss;

    if (binOp->isAssignmentOp()) {
        auto pairLhs = convertExpr(false);
        auto pairRhs = convertExpr(pairLhs.second);

        ss << "instr.AssignI(" << pairLhs.first << ", " << pairRhs.first << ")";
        return std::make_pair<std::string, bool>(ss.str(), false); // TODO: could be true
    }

    return std::make_pair<std::string, bool>("", false); // TODO: could be true

}

// void SlangGenChecker::handleTerminator(
//         const Stmt *terminator, std::unordered_        auto rhs = tib.main_stack.top();map<const Expr *, int> &visited,
//         unsigned int block_id) const {
//
//     if (terminator && isa<IfStmt>(terminator)) {
//         const Expr *condition = (cast<IfStmt>(terminator))->getCond();
//         llvm::errs() << "if ";
//         handleBinaryOperator(condition, visited, block_id);
//         llvm::errs() << "\n";
//     }
// }

//BOUND END  : handling_routines

//BOUND START: conversion_routines

// convert top of stack to SPAN IR.
// returns converted string, and false if the converted string is only a simple const/var expression.
std::pair<std::string, bool> SlangGenChecker::convertExpr(bool compound_receiver) const {
    std::stringstream ss;

    const Stmt *stmt = tib.main_stack.top();
    tib.main_stack.pop();

    switch(stmt->getStmtClass()) {
        case Stmt::IntegerLiteralClass: {
            ss << convertIntegerLiteral(cast<IntegerLiteral>(stmt)).first;
            return std::make_pair<std::string, bool>(ss.str(), false);
        }

        case Stmt::DeclRefExprClass: {
            ss << convertDeclRefExpr(cast<DeclRefExpr>(stmt)).first;
            return std::make_pair<std::string, bool>(ss.str(), false);
        }

        case Stmt::BinaryOperatorClass: {
            break;
        }

        default: {
            // error state
            llvm::errs() << "SLANG: ERROR: convertExpr: " << stmt->getStmtClassName() << "\n";
            stmt->dump();
            return std::make_pair<std::string, bool>("ERROR:convertExpr", false);
        }

        return std::make_pair<std::string, bool>("ERROR:convertExpr", false);
    }
} // convertExpr()

std::pair<std::string, bool> SlangGenChecker::convertVarDecl(const VarDecl *varDecl) const {
    std::stringstream ss;

    ss << "expr.VarE(\"" << tib.convertVarExpr((uint64_t)varDecl) << "\")";

    return std::make_pair<std::string, bool>(ss.str(), false);
}

std::pair<std::string, bool> SlangGenChecker::convertDeclRefExpr(const DeclRefExpr *dre) const {
    std::stringstream ss;

    const ValueDecl *valueDecl = dre->getDecl();
    if (isa<VarDecl>(valueDecl)) {
        auto varDecl = cast<VarDecl>(valueDecl);
        return convertVarDecl(varDecl);
    } else {
        llvm::errs() << "SLANG: ERROR: " << __func__ << ": Not a VarDecl.";
        return std::make_pair<std::string, bool>("", false);
    }
}

//BOUND END  : conversion_routines

//BOUND START: helper_functions

void SlangGenChecker::addStmtToCurrBlock(std::string stmt) const {
    // tib.bb_stmts[tib.curr_bb_id].push_back(stmt);

    std::vector<std::string> stmt_seq;
    stmt_seq = tib.bb_stmts[tib.curr_bb_id];
    stmt_seq.push_back(stmt);
    tib.bb_stmts[tib.curr_bb_id] = stmt_seq;
}

//BOUND END  : helper_functions

// Register the Checker
void ento::registerSlangGenChecker(CheckerManager &mgr) {
    mgr.registerChecker<SlangGenChecker>();
}
