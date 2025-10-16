#!/usr/bin/env python3
"""
Enhanced C++ Code Analyzer with Comprehensive Fallback Detection
Critical for detecting simplified implementations that could cause financial losses
"""

import os
import sys
import json
import hashlib
import argparse
import re
from pathlib import Path
from collections import defaultdict, Counter
from datetime import datetime
from typing import Dict, List, Set, Tuple, Optional, Any
import subprocess

# Try to import clang bindings
try:
    import clang.cindex as clang
except ImportError:
    print("Error: libclang Python bindings not found.")
    print("Install with: pip install libclang")
    sys.exit(1)

# Initialize clang
def init_clang():
    """Initialize clang library path"""
    possible_paths = [
        "/usr/lib/llvm-14/lib",
        "/usr/lib/llvm-13/lib",
        "/usr/lib/llvm-12/lib",
        "/usr/lib/llvm-11/lib",
        "/usr/lib/llvm-10/lib",
        "/usr/local/opt/llvm/lib",
        "/Library/Developer/CommandLineTools/usr/lib",
        "/opt/homebrew/opt/llvm/lib",
    ]
    
    for path in possible_paths:
        if os.path.exists(path):
            clang.Config.set_library_path(path)
            break

init_clang()

class FallbackDetector:
    """
    CRITICAL: Detect fallback mechanisms, simplified implementations, and stub code
    Zero tolerance for any code that doesn't perform its intended proper work
    """
    
    # Warning keywords in comments - comprehensive list
    FALLBACK_KEYWORDS = {
        # Primary fallback indicators
        'fallback', 'fall back', 'fall-back',
        'simplified', 'simplify', 'simple version',
        'approximate', 'approximation', 'approx',
        'workaround', 'work around', 'work-around',
        'temporary', 'temp fix', 'quick fix',
        'good enough', 'close enough', 'mostly works',
        'hack', 'hacky', 'kludge',
        'stub', 'stubbed', 'placeholder',
        'mock', 'mocked', 'fake', 'dummy',
        
        # Secondary indicators
        'for now', 'later', 'eventually',
        'not implemented', 'not complete',
        'partial', 'partially', 'incomplete',
        'basic', 'basic version', 'minimal',
        'rough', 'rough estimate', 'ballpark',
        'guess', 'guessing', 'assumption',
        'shortcut', 'cheat', 'bypass',
        'ignore', 'skip', 'omit',
        'default', 'hardcoded', 'hard-coded',
        'artificial', 'synthetic', 'made up'
    }
    
    # TODO/FIXME markers - expanded list
    TODO_MARKERS = {
        'TODO', 'FIXME', 'HACK', 'XXX', 'BUG', 'BROKEN',
        'INCOMPLETE', 'UNFINISHED', 'STUB', 'PLACEHOLDER',
        'TEMPORARY', 'REFACTOR', 'OPTIMIZE', 'REVIEW',
        'CHECKME', 'DOCME', 'TESTME', 'REMOVEME'
    }
    
    # Function names that suggest real work
    CRITICAL_FUNCTION_PATTERNS = [
        'calculate', 'compute', 'process', 'validate', 'analyze',
        'generate', 'execute', 'perform', 'update', 'initialize',
        'apply', 'transform', 'convert', 'parse', 'evaluate',
        'optimize', 'solve', 'determine', 'derive', 'extract',
        'build', 'create', 'construct', 'prepare', 'setup',
        'check', 'verify', 'authenticate', 'authorize', 'encrypt',
        'send', 'receive', 'transmit', 'fetch', 'retrieve',
        'save', 'load', 'store', 'persist', 'cache',
        'render', 'display', 'format', 'serialize', 'deserialize'
    ]
    
    # Financial/Trading specific critical patterns
    FINANCIAL_CRITICAL_PATTERNS = [
        'price', 'trade', 'order', 'position', 'portfolio',
        'risk', 'mrb', 'allocation', 'signal', 'strategy',
        'profit', 'loss', 'pnl', 'return', 'yield',
        'volatility', 'variance', 'sharpe', 'kelly',
        'backtest', 'forward', 'live', 'market', 'exchange'
    ]
    
    @staticmethod
    def analyze_function_for_fallbacks(cursor, file_path: str) -> List[Dict[str, Any]]:
        """Comprehensive fallback detection for a function"""
        issues = []
        
        # Run all detection methods
        issues.extend(FallbackDetector._detect_exception_fallbacks(cursor, file_path))
        issues.extend(FallbackDetector._detect_stub_implementations(cursor, file_path))
        issues.extend(FallbackDetector._detect_simplified_logic(cursor, file_path))
        issues.extend(FallbackDetector._detect_hardcoded_returns(cursor, file_path))
        issues.extend(FallbackDetector._detect_minimal_implementations(cursor, file_path))
        issues.extend(FallbackDetector._detect_conditional_fallbacks(cursor, file_path))
        issues.extend(FallbackDetector._detect_missing_dependencies(cursor, file_path))
        issues.extend(FallbackDetector._detect_suspicious_patterns(cursor, file_path))
        issues.extend(FallbackDetector._detect_empty_catch_blocks(cursor, file_path))
        issues.extend(FallbackDetector._detect_always_true_false_returns(cursor, file_path))
        
        return issues
    
    @staticmethod
    def _detect_exception_fallbacks(cursor, file_path: str) -> List[Dict]:
        """Detect try-catch blocks with fallback return values"""
        issues = []
        
        def find_try_catch_blocks(node):
            """Recursively find try-catch blocks"""
            # Check for try statement
            if node.kind == clang.CursorKind.COMPOUND_STMT:
                source = FallbackDetector._get_source_text(node, file_path)
                if source and 'try' in source and 'catch' in source:
                    # Analyze catch block for fallback patterns
                    lines = source.split('\n')
                    in_catch = False
                    catch_start_line = 0
                    
                    for i, line in enumerate(lines):
                        if 'catch' in line:
                            in_catch = True
                            catch_start_line = node.location.line + i
                        elif in_catch:
                            # Check for return statements in catch
                            if 'return' in line:
                                # Check what's being returned
                                if any(pattern in line for pattern in ['0', '0.0', 'false', 'nullptr', '""', "''", 'NULL']):
                                    issues.append({
                                        'type': 'exception_fallback_literal',
                                        'severity': 'CRITICAL',
                                        'function': cursor.spelling,
                                        'file': file_path,
                                        'line': catch_start_line,
                                        'message': "CRITICAL: Catch block returns literal/default value instead of re-throwing",
                                        'recommendation': 'Re-throw exception or crash with detailed error. NEVER return fallback values.',
                                        'code_snippet': line.strip()
                                    })
                                elif not 'throw' in line:
                                    issues.append({
                                        'type': 'exception_fallback_no_rethrow',
                                        'severity': 'CRITICAL',
                                        'function': cursor.spelling,
                                        'file': file_path,
                                        'line': catch_start_line,
                                        'message': "CRITICAL: Catch block returns without re-throwing exception",
                                        'recommendation': 'Must re-throw or provide comprehensive error handling',
                                        'code_snippet': line.strip()
                                    })
                            
                            # Check for logging without action
                            if any(log in line for log in ['log', 'LOG', 'print', 'cout', 'cerr']) and 'return' not in line and 'throw' not in line:
                                if i + 1 < len(lines) and 'return' not in lines[i + 1] and 'throw' not in lines[i + 1]:
                                    issues.append({
                                        'type': 'exception_silent_continuation',
                                        'severity': 'CRITICAL',
                                        'function': cursor.spelling,
                                        'file': file_path,
                                        'line': catch_start_line,
                                        'message': "CRITICAL: Catch block logs but continues execution silently",
                                        'recommendation': 'Must fail fast - either re-throw or return error state',
                                        'code_snippet': line.strip()
                                    })
            
            # Recurse through children
            for child in node.get_children():
                find_try_catch_blocks(child)
        
        find_try_catch_blocks(cursor)
        return issues
    
    @staticmethod
    def _detect_stub_implementations(cursor, file_path: str) -> List[Dict]:
        """Detect stub/placeholder implementations"""
        issues = []
        
        source = FallbackDetector._get_source_text(cursor, file_path)
        if not source:
            return issues
        
        # Check for TODO/FIXME markers
        has_todo = False
        todo_marker = None
        for marker in FallbackDetector.TODO_MARKERS:
            if marker in source.upper():
                has_todo = True
                todo_marker = marker
                break
        
        # Count meaningful statements
        stmt_count = 0
        has_return = False
        return_value = None
        
        for child in cursor.get_children():
            if child.kind == clang.CursorKind.COMPOUND_STMT:
                for stmt in child.get_children():
                    if stmt.kind == clang.CursorKind.RETURN_STMT:
                        has_return = True
                        # Try to get return value
                        return_source = FallbackDetector._get_source_text(stmt, file_path)
                        if return_source:
                            return_value = return_source.strip()
                    elif stmt.kind != clang.CursorKind.DECL_STMT:  # Don't count variable declarations
                        stmt_count += 1
        
        # Check if function is trivial
        is_trivial = stmt_count <= 1 and has_return
        
        # Check if returns constant
        returns_constant = False
        if return_value:
            # Check for literal patterns
            literal_patterns = [
                r'^return\s+\d+\.?\d*[fFlL]?\s*;?$',  # numeric literals
                r'^return\s+(true|false)\s*;?$',       # boolean literals
                r'^return\s+nullptr\s*;?$',            # nullptr
                r'^return\s+NULL\s*;?$',               # NULL
                r'^return\s+"[^"]*"\s*;?$',            # string literals
                r"^return\s+'[^']*'\s*;?$",            # char literals
                r'^return\s+\{\s*\}\s*;?$',            # empty initializer
            ]
            for pattern in literal_patterns:
                if re.match(pattern, return_value):
                    returns_constant = True
                    break
        
        # Check if this is a critical function name
        is_critical = any(pattern in cursor.spelling.lower() 
                         for pattern in FallbackDetector.CRITICAL_FUNCTION_PATTERNS)
        
        # Check if this is a financial function
        is_financial = any(pattern in cursor.spelling.lower() 
                          for pattern in FallbackDetector.FINANCIAL_CRITICAL_PATTERNS)
        
        # Detect various stub patterns
        if has_todo and returns_constant:
            severity = 'CRITICAL' if is_financial else 'HIGH'
            issues.append({
                'type': 'stub_implementation_todo',
                'severity': severity,
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': f"{severity}: Stub implementation with {todo_marker} marker and constant return",
                'recommendation': 'Implement proper logic immediately or remove this function',
                'todo_marker': todo_marker,
                'return_value': return_value
            })
        
        if is_critical and is_trivial:
            issues.append({
                'type': 'stub_implementation_trivial',
                'severity': 'CRITICAL',
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': f"CRITICAL: Critical function '{cursor.spelling}' has trivial/stub implementation",
                'recommendation': 'Implement complete logic - function name suggests complex work',
                'statement_count': stmt_count
            })
        
        if is_financial and returns_constant:
            issues.append({
                'type': 'financial_stub_implementation',
                'severity': 'CRITICAL',
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': f"CRITICAL: Financial function '{cursor.spelling}' returns hardcoded value",
                'recommendation': 'EXTREME RISK: Implement real calculation immediately - this affects real money',
                'return_value': return_value
            })
        
        # Check for empty body with just return
        if stmt_count == 0 and has_return and returns_constant:
            issues.append({
                'type': 'empty_stub',
                'severity': 'HIGH',
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': "HIGH: Function has empty body with only return statement",
                'recommendation': 'Implement function logic or remove if unnecessary',
                'return_value': return_value
            })
        
        # Check for unimplemented virtual functions
        if cursor.is_pure_virtual_method():
            # This is OK - pure virtual
            pass
        elif cursor.is_virtual_method() and is_trivial:
            issues.append({
                'type': 'virtual_stub',
                'severity': 'HIGH',
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': "HIGH: Virtual method has stub implementation",
                'recommendation': 'Implement virtual method properly or make it pure virtual'
            })
        
        return issues
    
    @staticmethod
    def _detect_simplified_logic(cursor, file_path: str) -> List[Dict]:
        """Detect simplified/approximate logic with warning comments"""
        issues = []
        
        source = FallbackDetector._get_source_text(cursor, file_path)
        if not source:
            return issues
        
        lower_source = source.lower()
        lines = source.split('\n')
        
        # Check each line for fallback keywords
        for keyword in FallbackDetector.FALLBACK_KEYWORDS:
            if keyword in lower_source:
                for i, line in enumerate(lines):
                    if keyword in line.lower():
                        # Check context - is this in a comment?
                        if '//' in line or '/*' in line or '*' in line.strip()[:2]:
                            # Get the next non-comment line to see what follows
                            next_code_line = None
                            for j in range(i + 1, min(i + 5, len(lines))):
                                if lines[j].strip() and not lines[j].strip().startswith('//'):
                                    next_code_line = lines[j].strip()
                                    break
                            
                            severity = 'CRITICAL'
                            # Extra critical if financial function
                            if any(fin in cursor.spelling.lower() for fin in FallbackDetector.FINANCIAL_CRITICAL_PATTERNS):
                                severity = 'CRITICAL'
                                message_prefix = "EXTREME RISK"
                            else:
                                message_prefix = "CRITICAL"
                            
                            issues.append({
                                'type': 'simplified_logic_comment',
                                'severity': severity,
                                'function': cursor.spelling,
                                'file': file_path,
                                'line': cursor.location.line + i,
                                'message': f"{message_prefix}: Simplified/fallback logic detected (keyword: '{keyword}')",
                                'recommendation': f"Implement proper logic immediately. Comment contains '{keyword}' indicating incomplete implementation",
                                'keyword': keyword,
                                'comment_line': line.strip(),
                                'following_code': next_code_line
                            })
                            break
        
        return issues
    
    @staticmethod
    def _detect_hardcoded_returns(cursor, file_path: str) -> List[Dict]:
        """Detect functions returning hardcoded constants"""
        issues = []
        
        # Check if function name suggests dynamic calculation
        suggests_calculation = any(
            pattern in cursor.spelling.lower()
            for pattern in ['get', 'calculate', 'compute', 'fetch', 'retrieve', 
                          'determine', 'find', 'query', 'load', 'read', 'derive',
                          'measure', 'evaluate', 'assess', 'estimate']
        )
        
        # Always check financial functions regardless of name
        is_financial = any(
            pattern in cursor.spelling.lower()
            for pattern in FallbackDetector.FINANCIAL_CRITICAL_PATTERNS
        )
        
        if not suggests_calculation and not is_financial:
            return issues
        
        # Analyze return statements
        for child in cursor.walk_preorder():
            if child.kind == clang.CursorKind.RETURN_STMT:
                # Get return statement source
                return_source = FallbackDetector._get_source_text(child, file_path)
                if not return_source:
                    continue
                
                # Check if returning a literal constant
                is_literal = False
                literal_value = None
                
                # Numeric literals
                if re.search(r'return\s+[\d\.\-\+]+[fFlL]?\s*;', return_source):
                    is_literal = True
                    literal_value = re.search(r'return\s+([\d\.\-\+]+[fFlL]?)\s*;', return_source).group(1)
                
                # String literals
                elif re.search(r'return\s+"[^"]*"\s*;', return_source):
                    is_literal = True
                    literal_value = re.search(r'return\s+("[^"]*")\s*;', return_source).group(1)
                
                # Boolean literals
                elif re.search(r'return\s+(true|false)\s*;', return_source):
                    is_literal = True
                    literal_value = re.search(r'return\s+(true|false)\s*;', return_source).group(1)
                
                # Nullptr/NULL
                elif re.search(r'return\s+(nullptr|NULL)\s*;', return_source):
                    is_literal = True
                    literal_value = re.search(r'return\s+(nullptr|NULL)\s*;', return_source).group(1)
                
                if is_literal:
                    # Check if there's any logic before the return
                    has_logic = False
                    for stmt in cursor.walk_preorder():
                        if stmt.kind in [clang.CursorKind.IF_STMT, clang.CursorKind.FOR_STMT,
                                        clang.CursorKind.WHILE_STMT, clang.CursorKind.SWITCH_STMT,
                                        clang.CursorKind.CALL_EXPR] and stmt != child:
                            has_logic = True
                            break
                    
                    if not has_logic:
                        severity = 'CRITICAL' if is_financial else 'HIGH'
                        message_prefix = "EXTREME RISK" if is_financial else "CRITICAL"
                        
                        issues.append({
                            'type': 'hardcoded_return',
                            'severity': severity,
                            'function': cursor.spelling,
                            'file': file_path,
                            'line': child.location.line,
                            'message': f"{message_prefix}: Function '{cursor.spelling}' returns hardcoded constant '{literal_value}' without calculation",
                            'recommendation': 'Implement proper calculation or use const/constexpr if truly constant',
                            'literal_value': literal_value,
                            'is_financial': is_financial
                        })
                
                break  # Only check first return for now
        
        return issues
    
    @staticmethod
    def _detect_minimal_implementations(cursor, file_path: str) -> List[Dict]:
        """Detect empty or minimal function bodies"""
        issues = []
        
        # Count different types of statements
        meaningful_stmts = 0
        total_stmts = 0
        has_only_return = False
        has_only_variable_decls = True
        
        for child in cursor.get_children():
            if child.kind == clang.CursorKind.COMPOUND_STMT:
                stmts = list(child.get_children())
                total_stmts = len(stmts)
                
                for stmt in stmts:
                    if stmt.kind != clang.CursorKind.RETURN_STMT and stmt.kind != clang.CursorKind.DECL_STMT:
                        meaningful_stmts += 1
                        has_only_variable_decls = False
                    elif stmt.kind == clang.CursorKind.RETURN_STMT:
                        if total_stmts == 1:
                            has_only_return = True
                    elif stmt.kind != clang.CursorKind.DECL_STMT:
                        has_only_variable_decls = False
        
        # Check if function name suggests complex work
        is_critical = any(
            pattern in cursor.spelling.lower()
            for pattern in FallbackDetector.CRITICAL_FUNCTION_PATTERNS
        )
        
        is_financial = any(
            pattern in cursor.spelling.lower()
            for pattern in FallbackDetector.FINANCIAL_CRITICAL_PATTERNS
        )
        
        # Detect various minimal patterns
        if is_critical and meaningful_stmts == 0:
            severity = 'CRITICAL' if is_financial else 'HIGH'
            issues.append({
                'type': 'empty_critical_implementation',
                'severity': severity,
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': f"{severity}: Critical function '{cursor.spelling}' has empty/minimal implementation",
                'recommendation': 'Implement proper logic immediately or remove function declaration',
                'meaningful_statements': meaningful_stmts,
                'total_statements': total_stmts
            })
        
        if has_only_return:
            issues.append({
                'type': 'return_only_implementation',
                'severity': 'HIGH',
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': f"HIGH: Function '{cursor.spelling}' contains only a return statement",
                'recommendation': 'Implement function logic or remove if unnecessary'
            })
        
        if has_only_variable_decls and meaningful_stmts == 0:
            issues.append({
                'type': 'declarations_only_implementation',
                'severity': 'MEDIUM',
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': f"MEDIUM: Function '{cursor.spelling}' only declares variables without using them",
                'recommendation': 'Complete implementation or remove unused function'
            })
        
        # Check for suspiciously short implementations of complex functions
        if is_financial and total_stmts < 3:
            issues.append({
                'type': 'minimal_financial_implementation',
                'severity': 'CRITICAL',
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': f"CRITICAL: Financial function '{cursor.spelling}' has suspiciously minimal implementation ({total_stmts} statements)",
                'recommendation': 'EXTREME RISK: Financial calculations require proper implementation',
                'statement_count': total_stmts
            })
        
        return issues
    
    @staticmethod
    def _detect_conditional_fallbacks(cursor, file_path: str) -> List[Dict]:
        """Detect if-else with fallback logic in one branch"""
        issues = []
        
        def analyze_if_statement(if_stmt, depth=0):
            """Analyze an if statement for fallback patterns"""
            children = list(if_stmt.get_children())
            if len(children) < 2:  # No else branch
                return
            
            # Get if and else branches
            condition = children[0] if len(children) > 0 else None
            then_branch = children[1] if len(children) > 1 else None
            else_branch = children[2] if len(children) > 2 else None
            
            if not else_branch:
                return
            
            # Check both branches for fallback patterns
            then_source = FallbackDetector._get_source_text(then_branch, file_path) if then_branch else ""
            else_source = FallbackDetector._get_source_text(else_branch, file_path) if else_branch else ""
            
            # Check for literal returns in branches
            then_returns_literal = any(pattern in then_source for pattern in 
                                      ['return 0', 'return 0.0', 'return false', 'return nullptr', 
                                       'return NULL', 'return ""', "return ''"])
            else_returns_literal = any(pattern in else_source for pattern in 
                                      ['return 0', 'return 0.0', 'return false', 'return nullptr',
                                       'return NULL', 'return ""', "return ''"])
            
            # Check for fallback comments
            then_has_fallback = any(keyword in then_source.lower() 
                                   for keyword in ['fallback', 'default', 'simplified', 'temporary', 'workaround'])
            else_has_fallback = any(keyword in else_source.lower() 
                                   for keyword in ['fallback', 'default', 'simplified', 'temporary', 'workaround'])
            
            if (then_returns_literal and then_has_fallback) or (else_returns_literal and else_has_fallback):
                branch_type = "then" if then_has_fallback else "else"
                issues.append({
                    'type': 'conditional_fallback_with_comment',
                    'severity': 'CRITICAL',
                    'function': cursor.spelling,
                    'file': file_path,
                    'line': if_stmt.location.line,
                    'message': f"CRITICAL: Conditional {branch_type} branch contains documented fallback logic",
                    'recommendation': 'Remove fallback branch. Both paths must perform proper work or fail explicitly',
                    'branch': branch_type
                })
            elif then_returns_literal or else_returns_literal:
                branch_type = "then" if then_returns_literal else "else"
                issues.append({
                    'type': 'conditional_fallback_literal',
                    'severity': 'HIGH',
                    'function': cursor.spelling,
                    'file': file_path,
                    'line': if_stmt.location.line,
                    'message': f"HIGH: Conditional {branch_type} branch returns literal/default value",
                    'recommendation': 'Verify this is intended behavior, not a fallback mechanism',
                    'branch': branch_type
                })
            
            # Check for different function calls in branches (possible fallback to simpler method)
            then_calls = re.findall(r'\b(\w+)\s*\(', then_source)
            else_calls = re.findall(r'\b(\w+)\s*\(', else_source)
            
            if then_calls and else_calls and then_calls != else_calls:
                # Check if one seems simpler than the other
                if any(simple in str(else_calls).lower() for simple in ['simple', 'basic', 'default', 'fallback']):
                    issues.append({
                        'type': 'conditional_different_methods',
                        'severity': 'HIGH',
                        'function': cursor.spelling,
                        'file': file_path,
                        'line': if_stmt.location.line,
                        'message': "HIGH: Conditional branches call different methods (possible fallback pattern)",
                        'recommendation': 'Ensure both branches provide equivalent functionality',
                        'then_calls': then_calls[:3],  # First 3 calls
                        'else_calls': else_calls[:3]
                    })
        
        # Find all if statements in the function
        for child in cursor.walk_preorder():
            if child.kind == clang.CursorKind.IF_STMT:
                analyze_if_statement(child)
        
        return issues
    
    @staticmethod
    def _detect_missing_dependencies(cursor, file_path: str) -> List[Dict]:
        """Detect functions that should orchestrate but don't call anything"""
        issues = []
        
        # Check if function name suggests orchestration
        orchestration_patterns = [
            'process', 'execute', 'perform', 'run', 'handle',
            'manage', 'coordinate', 'orchestrate', 'dispatch', 'route',
            'apply', 'invoke', 'trigger', 'initiate', 'start'
        ]
        
        calculation_patterns = [
            'calculate', 'compute', 'analyze', 'evaluate', 'determine',
            'derive', 'generate', 'transform', 'convert'
        ]
        
        is_orchestrator = any(
            pattern in cursor.spelling.lower()
            for pattern in orchestration_patterns
        )
        
        is_calculator = any(
            pattern in cursor.spelling.lower()
            for pattern in calculation_patterns
        )
        
        is_financial = any(
            pattern in cursor.spelling.lower()
            for pattern in FallbackDetector.FINANCIAL_CRITICAL_PATTERNS
        )
        
        if not is_orchestrator and not is_calculator:
            return issues
        
        # Count function calls and operations
        call_count = 0
        arithmetic_ops = 0
        returns_literal = False
        return_value = None
        
        for child in cursor.walk_preorder():
            if child.kind == clang.CursorKind.CALL_EXPR:
                call_count += 1
            elif child.kind == clang.CursorKind.BINARY_OPERATOR:
                # Check for arithmetic operations
                op_source = FallbackDetector._get_source_text(child, file_path)
                if op_source and any(op in op_source for op in ['+', '-', '*', '/', '%']):
                    arithmetic_ops += 1
            elif child.kind == clang.CursorKind.RETURN_STMT:
                return_source = FallbackDetector._get_source_text(child, file_path)
                if return_source:
                    # Check for literal return
                    if re.search(r'return\s+[\d\.\-]+[fFlL]?\s*;', return_source):
                        returns_literal = True
                        return_value = return_source.strip()
        
        # Detect missing orchestration
        if is_orchestrator and call_count == 0 and returns_literal:
            severity = 'CRITICAL' if is_financial else 'HIGH'
            issues.append({
                'type': 'missing_orchestration',
                'severity': severity,
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': f"{severity}: Orchestrator function '{cursor.spelling}' doesn't orchestrate anything",
                'recommendation': 'Implement proper orchestration logic or rename function to reflect actual behavior',
                'call_count': call_count,
                'return_value': return_value
            })
        
        # Detect missing calculation
        if is_calculator and arithmetic_ops == 0 and call_count == 0 and returns_literal:
            severity = 'CRITICAL' if is_financial else 'HIGH'
            message_prefix = "EXTREME RISK" if is_financial else "CRITICAL"
            issues.append({
                'type': 'missing_calculation',
                'severity': severity,
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': f"{message_prefix}: Calculator function '{cursor.spelling}' performs no calculations",
                'recommendation': 'Implement actual calculation logic - function name is misleading',
                'arithmetic_operations': arithmetic_ops,
                'return_value': return_value,
                'is_financial': is_financial
            })
        
        return issues
    
    @staticmethod
    def _detect_suspicious_patterns(cursor, file_path: str) -> List[Dict]:
        """Detect additional suspicious patterns that might indicate stubs"""
        issues = []
        
        source = FallbackDetector._get_source_text(cursor, file_path)
        if not source:
            return issues
        
        # Pattern 1: Functions that immediately return without any logic
        lines = [l.strip() for l in source.split('\n') if l.strip()]
        if len(lines) <= 3:  # Very short function
            # Check if it's just opening brace, return, closing brace
            if any('return' in line for line in lines):
                issues.append({
                    'type': 'suspiciously_short_function',
                    'severity': 'MEDIUM',
                    'function': cursor.spelling,
                    'file': file_path,
                    'line': cursor.location.line,
                    'message': f"MEDIUM: Function '{cursor.spelling}' is suspiciously short ({len(lines)} lines)",
                    'recommendation': 'Review if function is properly implemented',
                    'line_count': len(lines)
                })
        
        # Pattern 2: Multiple returns with different literals (inconsistent behavior)
        return_statements = re.findall(r'return\s+([^;]+);', source)
        if len(return_statements) > 1:
            # Check if returns are inconsistent
            unique_returns = set(return_statements)
            if len(unique_returns) > 1:
                # Check if any are literals
                literal_returns = []
                for ret in unique_returns:
                    if re.match(r'^[\d\.\-]+[fFlL]?$', ret.strip()) or ret.strip() in ['true', 'false', 'nullptr', 'NULL']:
                        literal_returns.append(ret.strip())
                
                if len(literal_returns) > 1:
                    issues.append({
                        'type': 'inconsistent_literal_returns',
                        'severity': 'HIGH',
                        'function': cursor.spelling,
                        'file': file_path,
                        'line': cursor.location.line,
                        'message': f"HIGH: Function returns different literal values in different paths",
                        'recommendation': 'Ensure consistent return behavior across all paths',
                        'literal_returns': literal_returns
                    })
        
        # Pattern 3: Commented out code (might indicate incomplete replacement)
        commented_code_patterns = [
            r'//.*\breturn\b',  # Commented return statements
            r'//.*\bcalculate\b',  # Commented calculations
            r'//.*\bprocess\b',  # Commented processing
            r'/\*.*?\*/',  # Block comments
        ]
        
        for pattern in commented_code_patterns:
            matches = re.findall(pattern, source, re.DOTALL)
            if matches and len(matches) > 2:  # Multiple commented sections
                issues.append({
                    'type': 'excessive_commented_code',
                    'severity': 'MEDIUM',
                    'function': cursor.spelling,
                    'file': file_path,
                    'line': cursor.location.line,
                    'message': "MEDIUM: Function contains significant commented-out code",
                    'recommendation': 'Remove commented code or implement properly',
                    'comment_count': len(matches)
                })
                break
        
        # Pattern 4: Magic numbers without explanation
        magic_numbers = re.findall(r'\b\d{2,}\b', source)  # Numbers with 2+ digits
        exclude_common = ['0', '1', '2', '10', '100', '1000']  # Common values
        magic_numbers = [n for n in magic_numbers if n not in exclude_common]
        
        if magic_numbers and cursor.spelling.lower() in ['calculate', 'compute', 'get']:
            issues.append({
                'type': 'unexplained_magic_numbers',
                'severity': 'MEDIUM',
                'function': cursor.spelling,
                'file': file_path,
                'line': cursor.location.line,
                'message': f"MEDIUM: Function contains unexplained magic numbers: {magic_numbers[:3]}",
                'recommendation': 'Use named constants or explain the significance of these values',
                'magic_numbers': magic_numbers[:5]
            })
        
        return issues
    
    @staticmethod
    def _detect_empty_catch_blocks(cursor, file_path: str) -> List[Dict]:
        """Detect empty catch blocks that silently swallow exceptions"""
        issues = []
        
        source = FallbackDetector._get_source_text(cursor, file_path)
        if not source or 'catch' not in source:
            return issues
        
        # Simple pattern matching for catch blocks
        catch_pattern = r'catch\s*\([^)]*\)\s*\{([^}]*)\}'
        matches = re.findall(catch_pattern, source, re.DOTALL)
        
        for i, catch_body in enumerate(matches):
            # Remove comments and whitespace
            clean_body = re.sub(r'//.*?\n', '', catch_body)
            clean_body = re.sub(r'/\*.*?\*/', '', clean_body, flags=re.DOTALL)
            clean_body = clean_body.strip()
            
            if not clean_body or clean_body == ';':
                issues.append({
                    'type': 'empty_catch_block',
                    'severity': 'CRITICAL',
                    'function': cursor.spelling,
                    'file': file_path,
                    'line': cursor.location.line,
                    'message': "CRITICAL: Empty catch block silently swallows exceptions",
                    'recommendation': 'Must handle exception properly or re-throw',
                    'catch_index': i + 1
                })
            elif len(clean_body) < 20 and 'throw' not in clean_body:
                issues.append({
                    'type': 'minimal_catch_block',
                    'severity': 'HIGH',
                    'function': cursor.spelling,
                    'file': file_path,
                    'line': cursor.location.line,
                    'message': "HIGH: Minimal catch block may not handle exception properly",
                    'recommendation': 'Ensure proper exception handling or re-throw',
                    'catch_body': clean_body[:50]
                })
        
        return issues
    
    @staticmethod
    def _detect_always_true_false_returns(cursor, file_path: str) -> List[Dict]:
        """Detect functions that always return true/false regardless of input"""
        issues = []
        
        source = FallbackDetector._get_source_text(cursor, file_path)
        if not source:
            return issues
        
        # Look for validation/check functions
        validation_patterns = ['validate', 'check', 'verify', 'is_valid', 'can_', 'should_', 'has_', 'is_']
        is_validation = any(pattern in cursor.spelling.lower() for pattern in validation_patterns)
        
        if not is_validation:
            return issues
        
        # Count return true/false statements
        return_true_count = len(re.findall(r'\breturn\s+true\s*;', source))
        return_false_count = len(re.findall(r'\breturn\s+false\s*;', source))
        total_returns = return_true_count + return_false_count
        
        # Check if function has any conditional logic
        has_conditions = any(keyword in source for keyword in ['if', 'switch', 'while', 'for'])
        
        # Detect always true/false
        if total_returns > 0:
            if return_true_count > 0 and return_false_count == 0 and not has_conditions:
                issues.append({
                    'type': 'always_returns_true',
                    'severity': 'HIGH',
                    'function': cursor.spelling,
                    'file': file_path,
                    'line': cursor.location.line,
                    'message': f"HIGH: Validation function '{cursor.spelling}' always returns true",
                    'recommendation': 'Implement actual validation logic or remove misleading function'
                })
            elif return_false_count > 0 and return_true_count == 0 and not has_conditions:
                issues.append({
                    'type': 'always_returns_false',
                    'severity': 'HIGH',
                    'function': cursor.spelling,
                    'file': file_path,
                    'line': cursor.location.line,
                    'message': f"HIGH: Validation function '{cursor.spelling}' always returns false",
                    'recommendation': 'Implement actual validation logic or remove misleading function'
                })
        
        return issues
    
    # Helper methods
    
    @staticmethod
    def _get_source_text(cursor, file_path: str) -> str:
        """Extract source text for a cursor with enhanced error handling"""
        try:
            # First try to get from extent
            if cursor.extent.start.file and cursor.extent.end.file:
                with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                    lines = f.readlines()
                    start_line = cursor.extent.start.line - 1
                    end_line = cursor.extent.end.line
                    
                    if 0 <= start_line < len(lines) and end_line <= len(lines):
                        # Also consider column positions for more accurate extraction
                        result_lines = []
                        for i in range(start_line, end_line):
                            if i == start_line and i == end_line - 1:
                                # Single line - use column positions
                                line = lines[i]
                                start_col = cursor.extent.start.column - 1
                                end_col = cursor.extent.end.column - 1
                                result_lines.append(line[start_col:end_col])
                            elif i == start_line:
                                # First line - from start column to end
                                line = lines[i]
                                start_col = cursor.extent.start.column - 1
                                result_lines.append(line[start_col:])
                            elif i == end_line - 1:
                                # Last line - from beginning to end column
                                line = lines[i]
                                end_col = cursor.extent.end.column - 1
                                result_lines.append(line[:end_col])
                            else:
                                # Middle lines - full line
                                result_lines.append(lines[i])
                        
                        return ''.join(result_lines)
            
            # Fallback: try to get tokens
            tokens = []
            for token in cursor.get_tokens():
                tokens.append(token.spelling)
            if tokens:
                return ' '.join(tokens)
            
        except Exception as e:
            # Silent fail - return empty string
            pass
        
        return ""
    
    @staticmethod
    def _returns_literal(return_stmt) -> bool:
        """Enhanced check if return statement returns a literal value"""
        # Try multiple methods to detect literal returns
        
        # Method 1: Check children cursor kinds
        for child in return_stmt.get_children():
            if child.kind in [
                clang.CursorKind.INTEGER_LITERAL,
                clang.CursorKind.FLOATING_LITERAL,
                clang.CursorKind.STRING_LITERAL,
                clang.CursorKind.CHARACTER_LITERAL,
                clang.CursorKind.CXX_BOOL_LITERAL_EXPR,
                clang.CursorKind.CXX_NULL_PTR_LITERAL_EXPR,
                clang.CursorKind.GNU_NULL_EXPR,
            ]:
                return True
        
        # Method 2: Check tokens
        tokens = list(return_stmt.get_tokens())
        if len(tokens) >= 2:  # At least "return" and a value
            value_token = tokens[1].spelling
            # Check for numeric literals
            if re.match(r'^[\d\.\-\+]+[fFlLuU]?$', value_token):
                return True
            # Check for known literals
            if value_token in ['true', 'false', 'nullptr', 'NULL']:
                return True
        
        return False
    
    @staticmethod
    def _has_only_return(cursor) -> bool:
        """Check if function has only a return statement"""
        stmts = []
        for child in cursor.get_children():
            if child.kind == clang.CursorKind.COMPOUND_STMT:
                stmts = [c for c in child.get_children()]
                break
        
        return len(stmts) == 1 and stmts[0].kind == clang.CursorKind.RETURN_STMT
    
    @staticmethod
    def _branch_returns_literal(branch) -> bool:
        """Check if branch returns a literal"""
        for child in branch.walk_preorder():
            if child.kind == clang.CursorKind.RETURN_STMT:
                return FallbackDetector._returns_literal(child)
        return False


class EnhancedCppAnalyzer:
    """Enhanced C++ code analyzer with comprehensive fallback detection"""
    
    def __init__(self, source_dir: str, output_file: str = "fallback_analysis_report.txt"):
        self.source_dir = Path(source_dir)
        self.output_file = output_file
        self.all_fallback_issues = []
        self.files_analyzed = 0
        self.functions_analyzed = 0
        
    def analyze(self, fail_on_fallback: bool = False, priority: str = "all", 
                include_metrics: bool = False, output_format: str = "text") -> int:
        """
        Run comprehensive fallback detection analysis
        
        Returns:
            0 if no critical issues or fail_on_fallback=False
            1 if critical issues found and fail_on_fallback=True
        """
        print("╔════════════════════════════════════════════════════════════════╗")
        print("║     Enhanced C++ Fallback Detection - ZERO TOLERANCE          ║")
        print("╚════════════════════════════════════════════════════════════════╝\n")
        
        # Find all C++ files
        cpp_files = list(self.source_dir.rglob("*.cpp")) + list(self.source_dir.rglob("*.h"))
        
        print(f"📂 Analyzing {len(cpp_files)} C++ files in {self.source_dir}")
        print(f"🎯 Priority filter: {priority}")
        print(f"⚠️  Fail on fallback: {'YES - Will block CI/CD' if fail_on_fallback else 'NO'}\n")
        
        # Analyze each file
        for cpp_file in cpp_files:
            self._analyze_file(str(cpp_file))
        
        # Filter by priority
        filtered_issues = self._filter_by_priority(self.all_fallback_issues, priority)
        
        # Generate reports
        self._generate_text_report(filtered_issues)
        self._generate_critical_report(filtered_issues)
        self._generate_json_report(filtered_issues)
        
        if include_metrics:
            self._generate_metrics_report(filtered_issues)
        
        # Print summary
        self._print_summary(filtered_issues, fail_on_fallback)
        
        # Determine exit code
        critical_count = sum(1 for issue in filtered_issues if issue['severity'] == 'CRITICAL')
        if fail_on_fallback and critical_count > 0:
            print("\n❌ BUILD FAILED: Critical fallback mechanisms detected!")
            print(f"   Fix {critical_count} CRITICAL issues before proceeding.\n")
            return 1
        
        return 0
    
    def _analyze_file(self, file_path: str):
        """Analyze a single C++ file for fallbacks"""
        try:
            # Parse the file with clang
            index = clang.Index.create()
            tu = index.parse(file_path, args=['-std=c++17'])
            
            self.files_analyzed += 1
            
            # Walk through all functions
            for cursor in tu.cursor.walk_preorder():
                if cursor.kind in [clang.CursorKind.FUNCTION_DECL, 
                                  clang.CursorKind.CXX_METHOD,
                                  clang.CursorKind.CONSTRUCTOR,
                                  clang.CursorKind.DESTRUCTOR]:
                    # Only analyze functions with definitions
                    if cursor.is_definition():
                        self.functions_analyzed += 1
                        issues = FallbackDetector.analyze_function_for_fallbacks(cursor, file_path)
                        self.all_fallback_issues.extend(issues)
                        
                        # Print immediate warnings for CRITICAL issues
                        for issue in issues:
                            if issue['severity'] == 'CRITICAL':
                                self._print_immediate_warning(issue)
        
        except Exception as e:
            print(f"⚠️  Error analyzing {file_path}: {e}")
    
    def _filter_by_priority(self, issues: List[Dict], priority: str) -> List[Dict]:
        """Filter issues by priority level"""
        if priority.lower() == "all":
            return issues
        elif priority.lower() == "critical":
            return [issue for issue in issues if issue['severity'] == 'CRITICAL']
        elif priority.lower() == "high":
            return [issue for issue in issues if issue['severity'] in ['CRITICAL', 'HIGH']]
        else:
            return issues
    
    def _print_immediate_warning(self, issue: Dict):
        """Print immediate console warning for CRITICAL issues"""
        print(f"\n🚨 CRITICAL FALLBACK DETECTED:")
        print(f"    Function: {issue['function']}")
        print(f"    File: {issue['file']}:{issue['line']}")
        print(f"    Type: {issue['type']}")
        print(f"    Message: {issue['message']}")
        print(f"    Action: {issue['recommendation']}")
        
        # Extra warning for financial functions
        if issue.get('is_financial', False):
            print(f"\n💸 EXTREME RISK: This affects REAL MONEY trading!")
            print(f"⚠️  FIX IMMEDIATELY before any production use!")
    
    def _generate_text_report(self, issues: List[Dict]):
        """Generate comprehensive text report"""
        with open(self.output_file, 'w') as f:
            f.write("╔════════════════════════════════════════════════════════════════╗\n")
            f.write("║     FALLBACK DETECTION ANALYSIS REPORT                         ║\n")
            f.write("╚════════════════════════════════════════════════════════════════╝\n\n")
            
            f.write(f"Analysis Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"Source Directory: {self.source_dir}\n")
            f.write(f"Files Analyzed: {self.files_analyzed}\n")
            f.write(f"Functions Analyzed: {self.functions_analyzed}\n")
            f.write(f"Total Issues Found: {len(issues)}\n\n")
            
            # Group by severity
            by_severity = defaultdict(list)
            for issue in issues:
                by_severity[issue['severity']].append(issue)
            
            f.write("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")
            f.write("SUMMARY BY SEVERITY\n")
            f.write("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n")
            
            for severity in ['CRITICAL', 'HIGH', 'MEDIUM', 'LOW']:
                count = len(by_severity[severity])
                if count > 0:
                    f.write(f"  {severity}: {count} issues\n")
            
            f.write("\n")
            
            # Detailed issues
            f.write("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")
            f.write("DETAILED ISSUES\n")
            f.write("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n")
            
            for i, issue in enumerate(issues, 1):
                f.write(f"[{i}] {issue['severity']} - {issue['type']}\n")
                f.write(f"    Function: {issue['function']}\n")
                f.write(f"    Location: {issue['file']}:{issue['line']}\n")
                f.write(f"    Message: {issue['message']}\n")
                f.write(f"    Recommendation: {issue['recommendation']}\n")
                
                # Additional context
                if 'code_snippet' in issue:
                    f.write(f"    Code: {issue['code_snippet']}\n")
                if 'return_value' in issue:
                    f.write(f"    Return Value: {issue['return_value']}\n")
                if 'is_financial' in issue and issue['is_financial']:
                    f.write(f"    ⚠️  FINANCIAL RISK: Affects real money trading!\n")
                
                f.write("\n")
        
        print(f"\n📄 Full report saved to: {self.output_file}")
    
    def _generate_critical_report(self, issues: List[Dict]):
        """Generate report with CRITICAL issues only"""
        critical_issues = [issue for issue in issues if issue['severity'] == 'CRITICAL']
        
        if not critical_issues:
            return
        
        critical_file = self.output_file.replace('.txt', '_CRITICAL_FALLBACKS.txt')
        
        with open(critical_file, 'w') as f:
            f.write("╔════════════════════════════════════════════════════════════════╗\n")
            f.write("║     CRITICAL FALLBACK ISSUES - IMMEDIATE ACTION REQUIRED      ║\n")
            f.write("╚════════════════════════════════════════════════════════════════╝\n\n")
            
            f.write(f"⚠️  {len(critical_issues)} CRITICAL FALLBACK MECHANISMS DETECTED\n")
            f.write(f"⚠️  These MUST be fixed before any production deployment\n")
            f.write(f"⚠️  Fallback mechanisms mask bugs and cause financial losses\n\n")
            
            for i, issue in enumerate(critical_issues, 1):
                f.write(f"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n")
                f.write(f"CRITICAL ISSUE #{i}\n")
                f.write(f"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n")
                f.write(f"Type: {issue['type']}\n")
                f.write(f"Function: {issue['function']}\n")
                f.write(f"Location: {issue['file']}:{issue['line']}\n")
                f.write(f"Message: {issue['message']}\n")
                f.write(f"Action Required: {issue['recommendation']}\n")
                
                if 'code_snippet' in issue:
                    f.write(f"\nProblematic Code:\n")
                    f.write(f"    {issue['code_snippet']}\n")
                
                if issue.get('is_financial', False):
                    f.write(f"\n💸 EXTREME FINANCIAL RISK\n")
                    f.write(f"This function affects REAL MONEY trading.\n")
                    f.write(f"Fix IMMEDIATELY before any live trading.\n")
                
                f.write("\n\n")
        
        print(f"🚨 Critical issues report saved to: {critical_file}")
    
    def _generate_json_report(self, issues: List[Dict]):
        """Generate machine-readable JSON report"""
        json_file = self.output_file.replace('.txt', '_data.json')
        
        report_data = {
            'analysis_date': datetime.now().isoformat(),
            'source_directory': str(self.source_dir),
            'files_analyzed': self.files_analyzed,
            'functions_analyzed': self.functions_analyzed,
            'total_issues': len(issues),
            'issues_by_severity': {
                'CRITICAL': len([i for i in issues if i['severity'] == 'CRITICAL']),
                'HIGH': len([i for i in issues if i['severity'] == 'HIGH']),
                'MEDIUM': len([i for i in issues if i['severity'] == 'MEDIUM']),
                'LOW': len([i for i in issues if i['severity'] == 'LOW']),
            },
            'issues': issues
        }
        
        with open(json_file, 'w') as f:
            json.dump(report_data, f, indent=2)
        
        print(f"📊 JSON data saved to: {json_file}")
    
    def _generate_metrics_report(self, issues: List[Dict]):
        """Generate detailed metrics report"""
        metrics_file = self.output_file.replace('.txt', '_metrics.json')
        
        # Calculate metrics
        files_with_issues = len(set(issue['file'] for issue in issues))
        functions_with_issues = len(set(issue['function'] for issue in issues))
        contamination_rate = (files_with_issues / self.files_analyzed * 100) if self.files_analyzed > 0 else 0
        
        # Group by type
        by_type = Counter(issue['type'] for issue in issues)
        
        # Financial risk count
        financial_risk_count = sum(1 for issue in issues if issue.get('is_financial', False))
        
        metrics = {
            'analysis_date': datetime.now().isoformat(),
            'files_analyzed': self.files_analyzed,
            'functions_analyzed': self.functions_analyzed,
            'files_with_issues': files_with_issues,
            'functions_with_issues': functions_with_issues,
            'contamination_rate_percent': round(contamination_rate, 2),
            'total_issues': len(issues),
            'financial_risk_count': financial_risk_count,
            'issues_by_type': dict(by_type),
            'issues_by_severity': {
                'CRITICAL': len([i for i in issues if i['severity'] == 'CRITICAL']),
                'HIGH': len([i for i in issues if i['severity'] == 'HIGH']),
                'MEDIUM': len([i for i in issues if i['severity'] == 'MEDIUM']),
                'LOW': len([i for i in issues if i['severity'] == 'LOW']),
            }
        }
        
        with open(metrics_file, 'w') as f:
            json.dump(metrics, f, indent=2)
        
        print(f"📈 Metrics saved to: {metrics_file}")
    
    def _print_summary(self, issues: List[Dict], fail_on_fallback: bool):
        """Print analysis summary to console"""
        print("\n╔════════════════════════════════════════════════════════════════╗")
        print("║                    ANALYSIS SUMMARY                            ║")
        print("╚════════════════════════════════════════════════════════════════╝\n")
        
        print(f"  Files Analyzed:     {self.files_analyzed}")
        print(f"  Functions Analyzed: {self.functions_analyzed}")
        print(f"  Total Issues:       {len(issues)}")
        
        # By severity
        critical = len([i for i in issues if i['severity'] == 'CRITICAL'])
        high = len([i for i in issues if i['severity'] == 'HIGH'])
        medium = len([i for i in issues if i['severity'] == 'MEDIUM'])
        low = len([i for i in issues if i['severity'] == 'LOW'])
        
        print(f"\n  Severity Breakdown:")
        print(f"    CRITICAL: {critical}")
        print(f"    HIGH:     {high}")
        print(f"    MEDIUM:   {medium}")
        print(f"    LOW:      {low}")
        
        # Financial risk
        financial_risk = sum(1 for issue in issues if issue.get('is_financial', False))
        if financial_risk > 0:
            print(f"\n  💸 Financial Risk Issues: {financial_risk}")
            print(f"     These affect REAL MONEY trading!")
        
        # Contamination rate
        files_with_issues = len(set(issue['file'] for issue in issues))
        contamination = (files_with_issues / self.files_analyzed * 100) if self.files_analyzed > 0 else 0
        print(f"\n  File Contamination: {contamination:.1f}% ({files_with_issues}/{self.files_analyzed} files)")
        
        # Final verdict
        print("\n" + "━" * 66)
        if critical == 0:
            print("✅ PASSED: No critical fallback mechanisms detected")
        else:
            print(f"❌ FAILED: {critical} critical fallback mechanisms detected")
            if fail_on_fallback:
                print("   Build will be blocked until these are fixed.")
        print("━" * 66 + "\n")


def main():
    parser = argparse.ArgumentParser(
        description="Enhanced C++ Fallback Detection - Zero Tolerance for Simplified Implementations",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Standard analysis
  python3 cpp_analyzer.py src/
  
  # CI/CD integration (fail on fallbacks)
  python3 cpp_analyzer.py src/ --fail-on-fallback
  
  # Focus on critical issues only
  python3 cpp_analyzer.py src/ --priority critical
  
  # Full metrics with JSON output
  python3 cpp_analyzer.py src/ --metrics --format json
        """
    )
    
    parser.add_argument('source_dir', help='Source directory to analyze')
    parser.add_argument('-o', '--output', default='fallback_analysis_report.txt',
                       help='Output report file (default: fallback_analysis_report.txt)')
    parser.add_argument('--fail-on-fallback', action='store_true',
                       help='Exit with error code if fallbacks detected (for CI/CD)')
    parser.add_argument('--priority', choices=['all', 'critical', 'high'], default='all',
                       help='Filter by priority level (default: all)')
    parser.add_argument('--metrics', action='store_true',
                       help='Generate detailed metrics report')
    parser.add_argument('--format', choices=['text', 'json'], default='text',
                       help='Output format (default: text)')
    
    args = parser.parse_args()
    
    # Run analysis
    analyzer = EnhancedCppAnalyzer(args.source_dir, args.output)
    exit_code = analyzer.analyze(
        fail_on_fallback=args.fail_on_fallback,
        priority=args.priority,
        include_metrics=args.metrics,
        output_format=args.format
    )
    
    sys.exit(exit_code)


if __name__ == '__main__':
    main()