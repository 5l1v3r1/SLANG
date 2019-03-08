#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""All instructions of IR

Note: use names in this module with module name: e.g. instr.ReadI...
TODO: complete the instruction type set.
"""
import logging
_log = logging.getLogger(__name__)
from typing import List, Set

from span.util.logger import LS
import span.ir.expr as expr
import span.ir.types as types
from span.ir.types import SourceLocationT
import span.util.util as util

InstrId = int
InstrCodeT = int

################################################
#BOUND START: instr_codes
################################################

# the order and ascending sequence is important
NOP_INSTR_IC                   = 0

BLOCK_ALL_INSTR_IC: InstrCodeT = 1
USE_INSTR_IC: InstrCodeT      = 2
COND_READ_INSTR_IC: InstrCodeT = 3
UNDEF_VAL_INSTR_IC: InstrCodeT = 4
FILTER_INSTR_IC: InstrCodeT    = 5
EX_READ_INSTR_IC: InstrCodeT   = 6

ASSIGN_INSTR_IC: InstrCodeT    = 10
RETURN_INSTR_IC: InstrCodeT    = 20
CALL_INSTR_IC: InstrCodeT      = 30
COND_INSTR_IC: InstrCodeT      = 40
GOTO_INSTR_IC: InstrCodeT      = 50

################################################
#BOUND END  : instr_codes
################################################

class InstrIT(types.AnyT):
  """Base type for all instructions."""
  def __init__(self,
               instrCode: InstrCodeT,
  ) -> None:
    if self.__class__.__name__.endswith("T"): super().__init__()
    self.type = types.Void  #default
    self.instrCode = instrCode
    self.loc: SourceLocationT = 0  # set in universe module

  def isNopInstr(self): return self.instrCode == NOP_INSTR_IC

  def isBlockInstr(self): return self.instrCode == BLOCK_ALL_INSTR_IC
  def isReadInstr(self): return self.instrCode == USE_INSTR_IC
  def isCondReadInstr(self): return self.instrCode == COND_READ_INSTR_IC
  def isUnDefInstr(self):return self.instrCode == UNDEF_VAL_INSTR_IC

  def isAssignInstr(self): return self.instrCode == ASSIGN_INSTR_IC
  def isCallInstr(self): return self.instrCode == CALL_INSTR_IC
  def isReturnInstr(self): return self.instrCode == RETURN_INSTR_IC
  def isCondInstr(self): return self.instrCode == COND_INSTR_IC
  def isGotoInstr(self): return self.instrCode == GOTO_INSTR_IC

class AssignI(InstrIT):
  """Assignment statement."""
  def __init__(self,
               lhs: expr.ExprET,
               rhs: expr.ExprET
  ) -> None:
    super().__init__(ASSIGN_INSTR_IC)
    self.lhs = lhs
    self.rhs = rhs

  def __eq__(self,
             other: 'AssignI'
  ) -> bool:
    if not isinstance(other, AssignI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.lhs == other.lhs:
      if LS: _log.warning("Lhs doesn't match: %s, %s", self, other)
      return False
    if not self.rhs == other.rhs:
      if LS: _log.warning("Rhs doesn't match: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"{self.lhs} = {self.rhs}"

  def __repr__(self): return self.__str__()

class CondI(InstrIT):
  """A conditional instruction."""
  def __init__(self,
               arg: expr.UnitET,
  ) -> None:
    super().__init__(COND_INSTR_IC)
    self.arg = arg

  def __eq__(self,
             other: 'CondI'
  ) -> bool:
    if not isinstance(other, CondI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.arg == other.arg:
      if LS: _log.warning("Arg doesn't match: %s, %s", self, other)
      return False
    return True

  def __str__(self):
    return f"if {self.arg}"

  def __repr__(self): return self.__str__()

class GotoI(InstrIT):
  """An unconditional jump (goto) instruction."""
  def __init__(self) -> None:
    super().__init__(GOTO_INSTR_IC)

  def __eq__(self, other: 'GotoI'):
    if not isinstance(other, GotoI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    return True

  def __str__(self): return f"goto"

class ReturnI(InstrIT):
  """Return statement."""
  def __init__(self,
               arg: expr.UnitET
  ) -> None:
    super().__init__(RETURN_INSTR_IC)
    self.arg = arg

  def __eq__(self,
             other: 'ReturnI'
  ) -> bool:
    if not isinstance(other, ReturnI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.arg == other.arg:
      if LS: _log.warning("Arg doesn't match: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"return {self.arg}"

class CallI(InstrIT):
  """Call statement."""
  def __init__(self,
               arg: expr.CallE
  ) -> None:
    super().__init__(CALL_INSTR_IC)
    self.arg = arg

  def __eq__(self,
             other: 'CallI'
  ) -> bool:
    if not isinstance(other, CallI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.arg == other.arg:
      if LS: _log.warning("Arg doesn't match: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"return {self.arg}"

################################################
#BOUND START: Special_Instructions
# These special instructions are used internally,
# but can be used in toy programs for testability.
################################################

class UseI(InstrIT):
  """Value of the variable is read."""
  def __init__(self,
               vars: Set[expr.VarE]
  ) -> None:
    super().__init__(USE_INSTR_IC)
    self.vars: Set[expr.VarE] = vars

  def __eq__(self,
             other: 'UseI'
  ) -> bool:
    if not isinstance(other, UseI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.vars == other.vars:
      if LS: _log.warning("Vars doesn't match: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"UseI({self.vars})"

  def __repr__(self): return self.__str__()

class ExReadI(InstrIT):
  """Only the given vars are exclusively read.

  All other vars are considered unread before this instruction.
  """
  def __init__(self,
               vars: Set[expr.VarE]
  ) -> None:
    super().__init__(EX_READ_INSTR_IC)
    self.vars: Set[expr.VarE] = vars

  def __eq__(self,
             other: 'ExReadI'
  ) -> bool:
    if not isinstance(other, ExReadI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.vars == other.vars:
      if LS: _log.warning("Vars doesn't match: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"ExReadI({self.vars})"

  def __repr__(self): return self.__str__()

class CondReadI(InstrIT):
  """Use of vars in rhs in lhs assignment.

  It implicitly blocks all forward/backward information.
  """
  def __init__(self,
               lhs: expr.VarE,
               rhs: Set[expr.VarE]
  ) -> None:
    super().__init__(COND_READ_INSTR_IC)
    self.lhs = lhs
    self.rhs = rhs

  def __eq__(self,
             other: 'CondReadI'
  ) -> bool:
    if not isinstance(other, CondReadI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.lhs == other.lhs:
      if LS: _log.warning("Lhs doesn't match: %s, %s", self, other)
      return False
    if not self.rhs == other.rhs:
      if LS: _log.warning("Rhs doesn't match: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"CondReadI({self.lhs}, {self.rhs})"

  def __repr__(self): return self.__str__()

class LiveI(InstrIT):
  """Set of vars live at this program point."""
  def __init__(self,
               vars: Set[expr.VarE] = None,
  ) -> None:
    super().__init__(FILTER_INSTR_IC)
    self.vars = vars if vars is not None else set()

  def __eq__(self,
             other: 'LiveI'
  ) -> bool:
    if not isinstance(other, LiveI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.vars == other.vars:
      if LS: _log.warning("Vars doesn't match: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"LiveI({self.vars})"

  def __repr__(self): return self.__str__()

class UnDefValI(InstrIT):
  """Variable takes an unknown/undefined value."""
  def __init__(self,
               lhs: expr.VarE # deliberately named lhs
  ) -> None:
    super().__init__(UNDEF_VAL_INSTR_IC)
    self.lhs: expr.VarE = lhs # deliberately named lhs

  def __eq__(self,
             other: 'UnDefValI'
  ) -> bool:
    if not isinstance(other, UnDefValI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.lhs == other.lhs:
      if LS: _log.warning("Vars doesn't match: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"UnDefValI({self.lhs})"

  def __repr__(self): return self.__str__()

class BlockInfoI(InstrIT):
  """Block all forward/backward information.

  Data flow information doesn't travel from IN to OUT or vice versa.
  """
  def __init__(self) -> None:
    super().__init__(BLOCK_ALL_INSTR_IC)

  def __eq__(self,
             other: 'BlockInfoI'
  ) -> bool:
    if not isinstance(other, BlockInfoI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    return True

  def __str__(self): return f"BlockInfoI()"

  def __repr__(self): return self.__str__()

class NopI(InstrIT):
  """A no operation instruction, for dummy nodes.

  For EmptyI, Host calls the identity transfer function of an analysis.
  """
  def __init__(self) -> None: super().__init__(NOP_INSTR_IC)

  def __eq__(self,
             other: 'NopI'
  ) -> bool:
    if not isinstance(other, NopI):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    return True

  def __str__(self): return "NopI()"

  def __repr__(self): return self.__str__()

################################################
#BOUND END  : Special_Instructions
################################################
