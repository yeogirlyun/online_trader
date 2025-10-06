# Online Trader Project Rules

**Established:** 2025-10-06  
**Status:** Mandatory for all AI models and contributors  
**Project Focus:** Online learning algorithms with ensemble PSM backend

---

## 🎯 **CRITICAL TESTING RULE: Real Market Data Only**

### **Mandatory Testing Standard**
**ALL testing must use real market data unless explicitly instructed otherwise.**

- **Default Data**: `data/QQQ_RTH_NH.bin` (real QQQ regular trading hours data)
- **Rationale**: Synthetic data provides misleading results and doesn't reflect real market patterns
- **Exception**: Only use synthetic data when explicitly requested for specific testing scenarios
- **Impact**: Ensures all performance measurements reflect actual trading conditions and market microstructure

**This rule supersedes all other testing preferences and must be followed for all online learning training, feature testing, and performance validation.**

---

## 🚫 **CRITICAL CODE MANAGEMENT RULE: No Duplicate Source Modules**

### **Mandatory Code Standards**
**NO duplicate source modules, files, or code blocks are allowed in the project.**

- **Direct Edits Only**: Make improvements by directly editing existing files
- **No Duplicate Files**: Never create `file_v2.py`, `file_new.cpp`, or similar duplicates
- **Git Version Control**: Use git for versioning, not different file names
- **Single Source of Truth**: Maintain one canonical version of each component
- **No Code Duplication**: Consolidate functionality into existing modules

**Examples of PROHIBITED practices:**
- ❌ `align_bars_4.py` (should edit `align_bars.py` directly)
- ❌ `strategy_ire_v2.cpp` (should edit `strategy_ire.cpp` directly)
- ❌ Copy-paste code blocks between files

### **🔧 IMPLEMENTATION GUIDANCE RULE**
**When provided with guidance containing code snippets, AI assistants must:**

- **✅ DIRECT MODIFICATION**: Directly modify existing modules in-place
- **✅ COMPLETE REWRITE**: Rewrite entire modules when necessary for improvements
- **❌ NO ENHANCED/ADVANCED/V2**: Never create `enhanced_`, `advanced_`, `_v2`, `_new`, etc. versions
- **❌ NO TEMPORARY DUPLICATES**: Never create temporary duplicate files "for comparison"
- **❌ NO PARALLEL VERSIONS**: Never maintain multiple versions of the same functionality

**Mandatory Process:**
1. **Identify existing module** that needs modification
2. **Edit directly** or rewrite entirely in the same file
3. **Validate functionality** after modification
4. **Remove any legacy code** that becomes obsolete

**Examples of CORRECT implementation:**
- ✅ Edit `src/training/quality_enforced_loss.cpp` directly
- ✅ Rewrite `include/strategy/transformer_model.h` entirely
- ✅ Modify existing class methods in-place

**This rule ensures clean architecture, maintainability, and prevents code divergence.**

---



#### ❌ **FORBIDDEN Actions**
- **CREATE** new testing modules, scripts, or frameworks
- **DUPLICATE** testing functionality that exists in online testing framework
- **BYPASS** the canonical testing interface
- **IGNORE** the online testing framework capabilities assessment

#### ⚠️ **CONDITIONAL Actions (Require Permission)**

**IF** the online testing framework lacks required functionality:

1. **STOP** and **ASK PERMISSION** before creating any new tester
2. **EXPLAIN** exactly what functionality is missing
3. **PROPOSE** adding the feature to the canonical online testing framework
4. **WAIT** for explicit authorization before proceeding

**Example Permission Request:**
```
The online testing framework doesn't support [SPECIFIC_FEATURE]. 
I need to [SPECIFIC_REQUIREMENT] for [USER_TASK].
Should I:
A) Add this feature to online_testing_framework?
B) Create a temporary wrapper around OnlineTester?
C) Use an alternative approach?
```

#### 🔄 **Temporary Tester Protocol (If Authorized)**

**IF** explicitly authorized to create a temporary tester:

1. **CREATE** in `/temp/` directory with clear naming: `temp_[purpose]_[date].cpp`
2. **DOCUMENT** why online testing framework was insufficient
3. **IMPLEMENT** as a wrapper around `OnlineTester` when possible
4. **USE** for the specific task only
5. **DELETE** immediately after task completion
6. **PROPOSE** integration into online testing framework for future use

**Example Temporary Tester:**
```cpp
// temp/temp_online_monte_carlo_20251006.cpp
#include "testing/online_testing_framework.h"

class TempMonteCarloWrapper {
public:
    std::vector<TestResult> monte_carlo_test(OnlineStrategy* strategy, 
                                           const MarketData& data, 
                                           int scenarios = 1000) {
        std::vector<TestResult> results;
        for (int i = 0; i < scenarios; ++i) {
            // Resample data, run OnlineTester, collect results
            OnlineTester tester(strategy, data);
            TestResult result = tester.run_backtest(resampled_data);
            results.push_back(result);
        }
        return analyze_results(results);
    }
};
```

### **🏗️ Online Testing Framework Capabilities Reference**

#### **Core Testing Functions**
- ✅ **Online Strategy Testing**: Single online learning strategy validation with comprehensive metrics
- ✅ **Ensemble PSM Testing**: Test ensemble position state machine with multiple strategies
- ✅ **Performance Analysis**: Monthly return, Sharpe ratio, drawdown, trade statistics
- ✅ **Walk-Forward Validation**: Time-series validation for online learning algorithms
- ✅ **Real-time Adaptation Testing**: Test incremental model updates and adaptation
- ✅ **Cost Modeling**: Realistic fees and slippage simulation
- ✅ **Portfolio Management**: Cash, position, leverage validation with ensemble PSM
- ✅ **Multi-Asset Support**: QQQ, TQQQ, SQQQ, PSQ with binary data format
- ✅ **Data Format Support**: Binary format with timezone handling
- ✅ **Validation & Verification**: Trade legality, cash checks, leverage limits

#### **Advanced Features**
- ✅ **Strategy Registration**: Plugin system for new online learning strategies
- ✅ **Configuration Management**: JSON-based strategy parameters
- ✅ **CLI Interface**: Complete command-line testing suite (`sentio_cli`)
- ✅ **Programmatic API**: C++ interface for custom workflows
- ✅ **Output Formats**: JSON, Markdown, CSV reporting
- ✅ **Performance Tracking**: Real-time metrics and adaptation monitoring

#### **Extensibility Points**
- ✅ **Strategy Interface**: Clean `OnlineStrategyBase` for any online learning algorithm
- ✅ **Execution Models**: Pluggable execution timing and logic
- ✅ **Cost Models**: Configurable fee and slippage calculations
- ✅ **Validation Rules**: Extensible portfolio and trade validation
- ✅ **Metrics Calculation**: Expandable performance metrics suite

### **🎯 Common Use Cases → Online Testing Framework Solutions**

| **Use Case** | **Old Approach** | **New Approach** |
|--------------|------------------|------------------|
| **Online Strategy Performance** | Custom testing scripts | `sentio_cli online-trade` |
| **Walk-Forward Testing** | Manual time-series testing | `sentio_cli walk-forward` |
| **Ensemble PSM Testing** | Multiple strategy coordination | `sentio_cli online-sanity-check` |
| **System Health Check** | Multiple health testers | `sentio_cli online-sanity-check` |
| **Strategy Validation** | Various compatibility testers | `test_online_trade` executable |
| **Benchmark Comparison** | Custom comparison scripts | Run multiple strategies with `OnlineTester` |
| **Parameter Optimization** | Custom optimization scripts | Wrap `OnlineTester` in optimization loop |

### **🚨 Violation Consequences**

**Creating unauthorized testers results in:**
- ❌ **Code Review Rejection** - Pull requests will be rejected
- ❌ **Technical Debt** - Increases maintenance burden
- ❌ **Inconsistency** - Breaks unified testing architecture
- ❌ **Audit Trail Loss** - Missing signal persistence and validation
- ❌ **Performance Issues** - Unoptimized, duplicate implementations

### **✅ Compliance Checklist**

Before any testing work, AI models must confirm:

- [ ] **Reviewed** online testing framework documentation and capabilities
- [ ] **Verified** online testing framework supports required functionality
- [ ] **Attempted** to solve the task using existing online testing framework features
- [ ] **Requested permission** if additional functionality is needed
- [ ] **Used** canonical testing interface for all testing operations
- [ ] **Avoided** creating duplicate or competing testing frameworks

### **📚 Required Reading**

All AI models must be familiar with:
1. **`README.md`** - Usage guide and examples
2. **`QUICKSTART.md`** - Quick start guide
3. **`TESTING_GUIDE.md`** - Testing framework capabilities
4. **`include/testing/`** - Testing framework headers
5. **`src/testing/`** - Testing framework implementation

### **🎯 Success Metrics**

- **Zero** unauthorized testing modules created
- **100%** testing tasks solved with online testing framework
- **Consistent** testing interfaces across all use cases
- **Complete** audit trail for all strategy testing
- **Maintainable** testing architecture with no duplication

---

## 📋 Documentation Policy

### **CRITICAL RULE: Focused Documentation System**

Online Trader maintains **FOCUSED** permanent documentation files:

1. **`README.md`** - Complete project overview, installation, and usage guide
2. **`QUICKSTART.md`** - Quick start guide for immediate setup
3. **`TESTING_GUIDE.md`** - Comprehensive testing framework documentation
4. **`PROJECT_STATUS.md`** - Current project status and development roadmap
5. **`MIGRATION_SUMMARY.md`** - Detailed migration report from sentio_trader

### **Mandatory Documentation Rules**

#### ✅ **ALLOWED Actions (Default Behavior)**
- **UPDATE** existing content in `README.md`
- **UPDATE** existing content in `QUICKSTART.md`  
- **UPDATE** existing content in `TESTING_GUIDE.md`
- **UPDATE** existing content in `PROJECT_STATUS.md`
- **UPDATE** existing content in `MIGRATION_SUMMARY.md`
- **REPLACE** outdated information with current codebase reflection
- **ENHANCE** existing sections with new features
- **CREATE** source code files (.cpp, .h, .json, .yaml) - Required to implement user instructions
- **CREATE** configuration files (.json, .yaml, .toml) - Required for functionality

#### ❌ **FORBIDDEN Actions (Default Behavior)**
- **CREATE** any new documentation files (.md, .rst, .txt) anywhere in the project
- **CREATE** additional README files in subdirectories  
- **CREATE** separate architecture documents
- **CREATE** feature-specific documentation files
- **CREATE** task completion summaries or reports
- **CREATE** implementation summaries or analysis documents
- **CREATE** bug reports or requirement documents
- **LEAVE** outdated information in documentation

### **CRITICAL RULE: No Unsolicited Document Creation**

**AI models must NEVER create any documentation files unless explicitly requested by the user.**

#### **Explicit Request Required For:**
- **Any new `.md`, `.rst`, or `.txt` files** - Must be specifically requested
- **Bug reports** - Only when user explicitly asks for bug analysis
- **Requirement documents** - Only when user explicitly asks for requirements
- **Analysis documents** - Only when user explicitly asks for analysis
- **Summary documents** - Only when user explicitly asks for summaries  
- **Report files** - Only when user explicitly asks for reports
- **Additional README files** - Only when user explicitly asks for specific README
- **Feature documentation** - Only when user explicitly asks for feature docs

#### **Never Create Without Explicit Request:**
- **Task completion summaries** - Provide brief chat summary instead
- **Implementation reports** - Discuss results in chat instead
- **Performance analysis documents** - Report findings in chat instead
- **System status documents** - Update architecture.md instead
- **Usage guides** - Update readme.md instead
- **Technical documentation** - Update architecture.md instead

### **Default AI Behavior Protocol**

When completing any task, AI models **MUST**:

1. **Complete the requested work** with source code changes
2. **Update permanent docs only** (`README.md`, `QUICKSTART.md`, `TESTING_GUIDE.md`, `PROJECT_STATUS.md`, `MIGRATION_SUMMARY.md`) if needed
3. **Provide brief summary in chat** - Never create summary documents
4. **Clean up temporary files** - Remove any working files created during implementation
5. **STOP** - Do not create any additional documentation files

#### **Example of Correct Behavior:**
```
✅ CORRECT:
- Implement online learning feature X in source code
- Update README.md with new component
- Update QUICKSTART.md with new usage instructions  
- Brief chat summary: "Online learning feature X implemented and documented"

❌ WRONG:
- Implement online learning feature X in source code
- Create ONLINE_FEATURE_X_IMPLEMENTATION_GUIDE.md
- Create ONLINE_FEATURE_X_USAGE_SUMMARY.md
- Create TASK_COMPLETION_REPORT.md
```

### **Temporary Document Policy**

#### **Megadocs (Only When Explicitly Requested)**
- **Location**: `megadocs/` folder only
- **Creation**: Only when user explicitly requests bug reports, requirement docs, or analysis
- **Tool**: Must use `tools/create_mega_document.py` 
- **Purpose**: Complex multi-file analysis when specifically requested

#### **Temporary Working Files (Allowed During Implementation)**
- **Debug files** (`debug_*.py`) - Must be removed after completion
- **Test files** (`test_*.py`) - Must be removed after completion  
- **Temporary data** (`temp_*.csv`) - Must be removed after completion
- **Working directories** (`temp_*/`) - Must be removed after completion

### **Documentation Update Requirements**

When making code changes, AI models **MUST**:

1. **Update README**: Reflect changes in `README.md` if system architecture changes
2. **Update QUICKSTART**: Reflect changes in `QUICKSTART.md` if user-facing changes occur
3. **Update Testing Guide**: Reflect changes in `TESTING_GUIDE.md` if testing framework changes
4. **Update Project Status**: Reflect changes in `PROJECT_STATUS.md` if project status changes
5. **Remove Outdated**: Delete obsolete information from existing docs
6. **Keep Current**: Ensure documentation matches current codebase
7. **No New Files**: Never create additional documentation files unless explicitly requested

---

## 🧹 **CRITICAL CODE EVOLUTION RULE: Remove Legacy Versions Immediately**

### **Mandatory Legacy Cleanup Standard**
**When an improved version of any source module is created, the old/legacy version MUST be completely removed immediately.**

#### **🎯 One Source Module Per Functionality**
- **Single Implementation**: Only ONE source module should exist for any specific functionality
- **Immediate Removal**: Old versions must be removed the moment improved versions are ready
- **No Coexistence**: Legacy and improved versions cannot exist simultaneously
- **Build System Cleanup**: All references in CMakeLists.txt, imports, and dependencies must be updated

#### **🔄 Evolution Process (MANDATORY)**

##### **Step 1: Create Improved Version**
```cpp
// ✅ CORRECT: Improve existing file directly
src/cli/tfm_trainer.cpp  // Edit this file directly for improvements
```

##### **Step 2: Test Improved Version**
```bash
# Verify improved version works completely
make && ./build/tfm_trainer  # Test thoroughly
```

##### **Step 3: Remove Legacy Immediately**
```bash
# ✅ MANDATORY: Remove all legacy versions immediately
rm src/cli/tfm_trainer_main.cpp           # Remove old trainer
rm src/cli/tfm_trainer_enhanced.cpp       # Remove intermediate version
rm src/cli/tfm_trainer_enhanced_v2.cpp    # Remove variation
rm include/training/enhanced_loss.h        # Remove old header
rm include/strategy/transformer_model_enhanced.h  # Remove obsolete interface
```

##### **Step 4: Update Build System**
```cmake
# Remove all references to deleted files in CMakeLists.txt
# Update target dependencies and linked libraries
# Ensure only current version is built
```

##### **Step 5: Verify Clean State**
```bash
# Confirm no build errors from missing files
make clean && make
# Verify functionality works with only the improved version
```

#### **📋 Examples of Immediate Removal**

##### **✅ CORRECT Evolution: TFM Trainer Pipeline**
```bash
# BEFORE (Multiple Versions - WRONG):
src/cli/tfm_trainer_main.cpp          # Original (broken)
src/cli/tfm_trainer_enhanced.cpp      # First improvement (mock data)
src/cli/tfm_trainer_real_data.cpp     # Real data version (slow)
src/cli/tfm_trainer_enhanced_v2.cpp   # Second improvement (config)
src/cli/tfm_trainer_efficient.cpp     # Final version (binary pipeline)

# AFTER (Single Version - CORRECT):
src/cli/tfm_trainer_efficient.cpp     # ✅ ONLY working version kept
# All others removed immediately: ✅ rm src/cli/tfm_trainer_*.cpp (except efficient)
```

##### **✅ CORRECT Evolution: Model Headers**
```bash
# BEFORE (Multiple Headers - WRONG):
include/strategy/transformer_model.h          # Original (feature_dim=126)
include/strategy/transformer_model_enhanced.h # Enhanced (feature_dim=128, wrong!)

# AFTER (Single Header - CORRECT):
include/strategy/transformer_model.h          # ✅ ONLY corrected version kept
# Enhanced version removed: ✅ rm include/strategy/transformer_model_enhanced.h
```

#### **🚨 Zero Tolerance Policy**

##### **❌ ABSOLUTELY FORBIDDEN**
- **Keeping "backup" versions** "just in case"
- **Commenting out** old code instead of removing files
- **Renaming** old files to `.old`, `.backup`, or similar
- **Moving** old files to archive directories
- **Leaving** unused imports or build references
- **Postponing** cleanup "until later" 

##### **✅ MANDATORY ACTIONS**
- **Delete files** completely using `rm` command
- **Remove CMakeLists.txt** references immediately
- **Update imports** in all dependent files
- **Test compilation** to ensure clean build
- **Verify functionality** of remaining version

#### **🔧 Cleanup Verification Checklist**

Before completing any code evolution work:
- [ ] **All legacy files deleted** with `rm` command
- [ ] **Build system updated** (CMakeLists.txt cleaned)
- [ ] **Imports updated** in all dependent files  
- [ ] **Clean compilation** successful (`make clean && make`)
- [ ] **Functionality verified** with improved version only
- [ ] **No broken references** to removed files
- [ ] **No dead code** or commented-out alternatives

#### **🎯 Benefits of Immediate Cleanup**

1. **Eliminates Confusion**: Developers always know which version to use
2. **Prevents Regression**: No risk of accidentally using obsolete versions
3. **Reduces Technical Debt**: Smaller, cleaner codebase to maintain
4. **Improves Build Speed**: Fewer files to compile and link
5. **Ensures Consistency**: Single implementation enforces standard behavior
6. **Simplifies Debugging**: Fewer code paths and fewer potential issues

#### **📚 Real-World Success: Recent TFM Cleanup (September 2025)**

**Problem**: 14 obsolete TFM source modules creating confusion and build issues
```bash
# REMOVED (All Obsolete):
src/cli/tfm_trainer_main.cpp           # ❌ Broken (missing headers)
src/cli/tfm_trainer_enhanced.cpp       # ❌ Mock data (obsolete)
src/cli/tfm_trainer_enhanced_v2.cpp    # ❌ Superseded
src/cli/tfm_trainer_real_data.cpp      # ❌ Slow (50x slower)
src/cli/tfm_trainer_with_validation.cpp # ❌ Experimental
include/strategy/transformer_model_enhanced.h # ❌ Wrong dimensions
include/training/enhanced_loss.h        # ❌ Replaced by quality_enforced_loss.h
include/training/prediction_monitor.h   # ❌ Unused
... and 6 more obsolete files
```

**Solution**: Applied immediate cleanup rule
```bash
# KEPT (Only Working Versions):
src/cli/tfm_trainer_efficient.cpp      # ✅ 50x faster binary pipeline
src/cli/preprocess_data.cpp             # ✅ Data preprocessing utility  
include/training/quality_enforced_loss.h # ✅ Current loss function
src/strategy/transformer_model.cpp      # ✅ Correct implementation
```

**Result**:
- ✅ **Zero Confusion**: Developers know exactly which files to use
- ✅ **Clean Build**: No broken references or missing dependencies  
- ✅ **50x Performance**: Only efficient versions remain
- ✅ **Maintainable Code**: Single source of truth for each component

#### **🚨 Enforcement**

**This rule is MANDATORY and will be enforced through:**
- **Code Reviews**: All pull requests checked for legacy file removal
- **Build Verification**: CI/CD must pass with clean builds only
- **Documentation Updates**: Architecture docs must reflect single versions
- **Automated Scanning**: Scripts to detect duplicate functionality

**Violation Consequences:**
- ❌ **Immediate Rejection**: Pull requests with legacy files will be rejected
- ❌ **Build Failures**: Compilation must succeed with improved versions only
- ❌ **Technical Debt**: Legacy files create maintenance burden and confusion

### **🏁 Summary: One Module, One Job, One Truth**

**Every functionality must have exactly ONE source module doing the job. When you improve it, the old one dies immediately. No exceptions, no excuses, no "keeping it just in case."**

---

## 🏗️ Architecture Rules

### **CRITICAL ARCHITECTURAL CONTRACT (Mandatory)**

**ALL backend systems (ensemble PSM, online learning, testing) MUST be strategy-agnostic and follow these principles:**

#### **🔒 RULE 1: Strategy-Agnostic Backend**
- **Ensemble PSM**: Must work with ANY online learning strategy implementing OnlineStrategyBase interface
- **Online Learning**: Must work with ANY online learning algorithm configuration
- **Testing Framework**: Must work with ANY strategy configuration
- **NO** strategy names, types, or strategy-specific logic in core systems
- **ALL** strategy behavior controlled via OnlineStrategyBase virtual methods

#### **🔒 RULE 2: OnlineStrategyBase API Completeness**
- **ALL** strategy behavior expressed through OnlineStrategyBase virtual methods
- **Extension Pattern**: Add virtual methods to OnlineStrategyBase, NOT core system modifications
- **Feature Flags**: Use boolean flags like `requires_ensemble_psm()` for optional behaviors
- **Configuration Objects**: Strategy preferences via config structs (OnlineConfig, etc.)

#### **🔒 RULE 3: Online Learning Optimization Mandate**
- **Real-time Adaptation**: Always enable incremental model updates
- **Ensemble Integration**: Use ensemble PSM for multi-strategy coordination
- **Performance Tracking**: Track adaptation performance and model evolution
- **Position Integrity**: Never allow negative positions or conflicting long/short positions

#### **🔒 RULE 4: Architectural Enforcement**
```cpp
// ✅ CORRECT: Strategy controls behavior via virtual methods
if (strategy->requires_ensemble_psm()) {
    // Use ensemble PSM path
} else {
    // Use single strategy path
}

// ❌ WRONG: Strategy-specific logic in backend
if (strategy->get_name() == "sgd") {
    // SGD-specific logic - FORBIDDEN
}
```

#### **🔒 RULE 5: Extension Protocol**
**BEFORE** modifying ensemble PSM/online learning/testing, ask:
1. Can this be achieved by extending OnlineStrategyBase API?
2. Is this change strategy-agnostic?
3. Does this maintain backward compatibility?
4. Will this work for all current and future online learning strategies?

**If ANY answer is "No", extend OnlineStrategyBase instead.**

#### **🔒 RULE 6: Code Review Checklist**
- [ ] **No strategy names** in ensemble PSM/online learning/testing code
- [ ] **All behavior** controlled via OnlineStrategyBase virtual methods
- [ ] **Backward compatible** with existing strategies
- [ ] **Real-time adaptation** maintained
- [ ] **Ensemble coordination** for multi-strategy scenarios
- [ ] **Position integrity** preserved (no negative/conflicting positions)

### **System Architecture Principles**

1. **Online Learning Focus**: All trading logic optimized for real-time adaptation
2. **Ensemble PSM**: Multi-strategy coordination with dynamic weighting
3. **Event-Driven**: Asynchronous, non-blocking operations
4. **CLI Integration**: All features accessible via sentio_cli commands
5. **Performance Tracking**: All algorithms must have adaptation monitoring
6. **Strategy-Agnostic Core**: Backend systems work with any OnlineStrategyBase implementation

### **Code Organization**

```
online_trader/
├── README.md               # Project overview and usage
├── QUICKSTART.md           # Quick start guide
├── TESTING_GUIDE.md        # Testing framework documentation
├── PROJECT_STATUS.md       # Current project status
├── MIGRATION_SUMMARY.md    # Migration from sentio_trader
├── include/                # Header files
│   ├── strategy/           # Strategy framework (OnlineStrategyBase)
│   ├── backend/            # Ensemble PSM and portfolio management
│   ├── learning/           # Online learning algorithms
│   ├── cli/                # Command-line interface
│   ├── testing/            # Testing framework
│   └── validation/         # Validation framework
├── src/                    # Implementation files (mirrors include/)
├── config/                 # Configuration files
├── data/                   # Market data (binary format)
└── tools/                  # Utility programs
```

### **Component Integration Rules**

1. **New Online Strategies**: Must integrate with OnlineStrategyBase interface
2. **CLI Features**: Must integrate with existing sentio_cli command system
3. **Performance Tracking**: Must provide real-time adaptation metrics
4. **Configuration**: Must use existing JSON config system
5. **Testing**: Must integrate with online testing framework

---

## 📁 Project Directory Structure

### **Online Trader Repository Structure**

Online Trader uses a **focused single repository** optimized for online learning:

```
online_trader/                   # Main project root
├── sentio_cli                   # Main CLI executable
├── test_online_trade           # Online learning test tool
├── include/                     # Header files
│   ├── strategy/                # Strategy framework (OnlineStrategyBase)
│   ├── backend/                 # Ensemble PSM and portfolio management
│   ├── learning/                # Online learning algorithms
│   ├── cli/                     # Command-line interface
│   ├── testing/                 # Testing framework
│   └── validation/              # Validation framework
├── src/                         # Implementation files (mirrors include/)
├── config/                      # Configuration files
├── data/                        # Market data (binary format)
└── tools/                       # Utility programs
```

### **Online Learning Integration Rules**

#### ✅ **ALLOWED Actions**
- **CONSOLIDATE** all online learning strategies under OnlineStrategyBase interface
- **CREATE** new online learning algorithms (e.g., SGD, Online Gradient Boosting)
- **MAINTAIN** separate configuration files for different strategies
- **ENSURE** all strategies work with ensemble PSM backend

#### ❌ **FORBIDDEN Actions**
- **CREATE** separate repositories for different online learning approaches
- **SCATTER** strategy files across multiple root directories
- **BREAK** compatibility with ensemble PSM backend
- **DUPLICATE** common online learning functionality across strategies

---

## 🤖 AI Model Guidelines

### **When Working on Online Trader**

#### **Documentation Policy (MANDATORY)**
```cpp
// DEFAULT BEHAVIOR - Focused documentation files:
1. README.md - Project overview and usage changes only
2. QUICKSTART.md - User-facing quick start changes only
3. TESTING_GUIDE.md - Testing framework changes only
4. PROJECT_STATUS.md - Project status updates only
5. MIGRATION_SUMMARY.md - Migration documentation only

// ALLOWED by default:
- Source code files (.cpp, .h, .json, .yaml, etc.)
- Configuration files (.json, .yaml, .toml)
- Brief chat summaries of completed work

// FORBIDDEN by default:
- Any new documentation files (.md, .rst, .txt)
- Task completion reports or summaries
- Implementation guides or analysis documents
- Bug reports or requirement documents
- Additional README files anywhere
- Feature-specific documentation

// ONLY create documentation when user EXPLICITLY requests:
- "Create a bug report for X"
- "Write a requirements document for Y" 
- "Generate analysis documentation for Z"
- "Create a usage guide for feature A"
```

#### **Default Response Behavior**
```cpp
// When completing tasks:
✅ DO: Implement requested functionality in source code
✅ DO: Update README.md only if project overview changes
✅ DO: Update QUICKSTART.md only if user-facing functionality changes
✅ DO: Update TESTING_GUIDE.md only if testing framework changes
✅ DO: Provide brief summary in chat response
✅ DO: Clean up any temporary files created during work

❌ DON'T: Create any .md files without explicit request
❌ DON'T: Create implementation summaries or reports
❌ DON'T: Create completion reports or task summaries
❌ DON'T: Create usage guides (update readme.md instead)
❌ DON'T: Create technical documentation (update existing docs instead)
❌ DON'T: Create analysis documents (discuss in chat instead)
```

#### **Command Execution Policy (Explicit Instruction Required)**
```cpp
// When the USER explicitly instructs to run programs or shell commands:
✅ DO: Execute the command immediately within the chat session's shell
✅ DO: Use non-interactive flags (e.g., --yes) and avoid prompts
✅ DO: Pipe paged output to cat to prevent blocking (e.g., ... | cat)
✅ DO: Run long-lived processes in background when appropriate
✅ DO: Capture and report stdout/stderr and exit codes in chat
✅ DO: Prefer project root absolute paths

// Safety & scope
✅ DO: Limit execution to the project workspace and intended tools
❌ DON'T: Execute destructive/system-wide commands unless explicitly stated
❌ DON'T: Require interactive input mid-run; fail fast and report instead

// This policy supersedes prior guidance about not running commands in chat
// when explicit user instruction to execute is given.
```

#### **Code Changes**
```cpp
// Follow these patterns:
1. Integrate with existing online learning framework
2. Add CLI commands for new features
3. Implement performance tracking for adaptation
4. Use existing JSON configuration system
5. Follow established error handling patterns
```

#### **Testing Requirements**
```cpp
// Before completing work:
1. Test CLI integration
2. Verify online learning performance tracking
3. Confirm ensemble PSM coordination
4. Validate configuration system
5. Update relevant documentation files
```

---

## 🚫 Code Duplication Prevention Rules

### **CRITICAL PRINCIPLE: No Duplicate Modules**

**Code duplication is pure evil and must be avoided at all costs.**

#### **File Naming Rules (MANDATORY)**

##### ✅ **ALLOWED Naming Patterns**
```cpp
// Descriptive, specific names that indicate exact purpose:
online_sgd_strategy.cpp          // SGD online learning strategy
ensemble_position_manager.cpp    // Ensemble PSM management
online_predictor.cpp              // Online learning predictor
signal_processor.cpp              // Signal processing
cli_command.cpp                   // CLI command implementation
```

##### ❌ **FORBIDDEN Naming Patterns**
```cpp
// Vague adjectives that create confusion:
advanced_*.cpp          // What makes it "advanced"?
enhanced_*.cpp          // Enhanced compared to what?
optimized_*.cpp         // All code should be optimized
improved_*.cpp          // Improved from what version?
fixed_*.cpp             // Fixes should overwrite, not duplicate
v2_*.cpp, v3_*.cpp       // Version numbers in filenames
final_*.cpp             // Nothing is ever truly final
new_*.cpp               // Everything was new once
better_*.cpp            // Subjective and meaningless
```

#### **Module Evolution Rules**

##### **Rule 1: Overwrite, Don't Duplicate**
```cpp
// WRONG: Creating new versions
online_sgd_strategy.cpp          // Original
advanced_online_sgd_strategy.cpp // ❌ FORBIDDEN - creates confusion
enhanced_online_sgd_strategy.cpp // ❌ FORBIDDEN - which one to use?

// RIGHT: Evolve in place
online_sgd_strategy.cpp          // ✅ Single source of truth
// When improving: edit online_sgd_strategy.cpp directly
```

##### **Rule 2: Specific Names for Different Behavior**
```cpp
// WRONG: Vague adjectives
signal_processor.cpp         // Original
advanced_signal_processor.cpp // ❌ What's "advanced"?

// RIGHT: Specific characteristics
signal_processor.cpp         // General signal processing
momentum_signal_processor.cpp // ✅ Momentum-based signals
ml_signal_processor.cpp      // ✅ Machine learning signals
```

##### **Rule 3: Temporary Files Must Be Cleaned**
```cpp
// During development, temporary files are acceptable:
debug_*.cpp              // For debugging only
test_*.cpp               // For testing only
temp_*.cpp               // For temporary work

// But MUST be removed before completion:
rm debug_*.cpp           // Clean up when done
rm test_*.cpp            // Remove temporary tests
rm temp_*.cpp            // Delete temporary files
```

#### **Implementation Guidelines**

##### **When Improving Existing Code:**
1. **Edit the original file directly**
2. **Do NOT create new versions with adjectives**
3. **Use git for version history, not filenames**
4. **Test thoroughly before overwriting**
5. **Update imports if class names change**

##### **When Adding New Functionality:**
1. **Ask: Is this truly different behavior?**
2. **If same purpose: enhance existing file**
3. **If different purpose: use specific descriptive name**
4. **Never use vague adjectives like "advanced" or "enhanced"**

##### **Examples of Correct Evolution:**

```python
# Scenario: Improving PPO trainer
# WRONG:
ppo_trainer.py              # Original
advanced_ppo_trainer.py     # ❌ Creates confusion

# RIGHT:
ppo_trainer.py              # ✅ Evolved in place

# Scenario: Adding different signal processing
# WRONG:
signal_processor.py         # Original  
enhanced_signal_processor.py # ❌ Vague adjective

# RIGHT:
signal_processor.py         # Base processor
momentum_signal_processor.py # ✅ Specific: momentum-based
mean_reversion_processor.py  # ✅ Specific: mean reversion
```

#### **Enforcement Rules**

##### **AI Models MUST:**
1. **Check for existing similar files before creating new ones**
2. **Use specific, descriptive names that indicate exact purpose**
3. **Never use vague adjectives (advanced, enhanced, optimized, etc.)**
4. **Overwrite existing files when improving functionality**
5. **Remove temporary/debug files after completion**
6. **Update all imports when renaming files**

##### **Automatic Violations:**
Any file with these patterns will be **automatically rejected**:
- `*advanced*`
- `*enhanced*`
- `*optimized*`
- `*improved*`
- `*fixed*`
- `*v2*`, `*v3*`, etc.
- `*final*`
- `*new*`
- `*better*`

#### **Code Review Checklist**

Before completing any work, verify:
- [ ] No duplicate modules with similar functionality
- [ ] No vague adjectives in filenames
- [ ] All temporary/debug files removed
- [ ] Imports updated for any renamed files
- [ ] Single source of truth for each functionality
- [ ] File names clearly indicate specific purpose
- [ ] **Run duplicate detection scan: `python3 tools/dupdef_scan.py --fail-on-issues`**

#### **Automated Duplicate Detection**

**MANDATORY:** All code changes must pass the duplicate definition scanner:

```bash
# Run before committing any code:
python3 tools/dupdef_scan.py --fail-on-issues

# For detailed report:
python3 tools/dupdef_scan.py --out duplicate_report.txt

# For JSON output:
python3 tools/dupdef_scan.py --json --out duplicate_report.json
```

**The scanner detects:**
- Duplicate class names across files
- Duplicate method names within classes
- Duplicate functions within modules
- Overload groups without implementations
- Syntax errors

**Zero tolerance policy:** Any duplicates found must be resolved before code completion.

#### **Real-World Example: PPO Cleanup (August 2025)**

**Problem:** PPO codebase had accumulated 20+ duplicate files:
```
❌ BEFORE (Confusing mess):
advanced_ppo_trainer.py
enhanced_ppo_network.py
sentio_ppo_integration.py
train_ppo_fixed.py
train_ppo_fixed_final.py
train_ppo_10_percent_monthly.py
train_ppo_optimized.py
apply_ppo_fixes_immediately.py
debug_ppo_rewards.py
test_ppo_fixes.py
... and 10+ more duplicates
```

**Solution:** Applied these rules rigorously:
```
✅ AFTER (Clean, clear):
models/ppo_trainer.py      # Single PPO training system
models/ppo_network.py      # Single neural network
models/ppo_integration.py  # Single integration module
models/ppo_trading_agent.py # Base agent system
train_ppo.py               # Single training script
```

**Result:** 
- 20+ files reduced to 5 essential files
- Zero confusion about which file to use
- All functionality preserved and improved
- Clean, maintainable codebase

**This is the standard all future development must follow.**

---

## 📁 File Management Rules

### **Documentation Cleanup**

The following files have been **REMOVED** and should **NEVER** be recreated:

#### **Removed Files**
- `INTEGRATION_COMPLETE.md`
- `ALGORITHM_COMPARISON_GUIDE.md`
- `README_ENHANCED.md`
- All files in `analysis/reports/*.md`
- All files in `docs/financeai/*.md`
- `ui/README_*.md`
- `req_requests/*.md`
- `tools/*.md`
- `entity/README.md`
- `trader-bot/README.md`
- `trader-bot/overview.md`

#### **Cleanup Commands**
```bash
# These files are removed and should not be recreated
rm -f INTEGRATION_COMPLETE.md
rm -f ALGORITHM_COMPARISON_GUIDE.md
rm -f README_ENHANCED.md
rm -rf analysis/reports/*.md
rm -rf docs/financeai/
rm -f ui/README_*.md
rm -f req_requests/*.md
rm -f tools/GUI_AND_MODEL_REQUIREMENTS.md
rm -f tools/FINANCEAI_MEGA_DOC.md
rm -f entity/README.md
rm -f trader-bot/README.md
rm -f trader-bot/overview.md
```

### **Allowed Non-Documentation Files**

These files are **PERMITTED** and serve specific functions:
- `PROJECT_RULES.md` (this file - project governance)
- `requirements*.txt` (dependency management)
- `config/*.yaml` (configuration files)
- `config/*.json` (configuration files)
- `.env.example` (environment template)

### **Git Repository Exclusions**

The following directories and files are **TEMPORARY** and must **NEVER** be committed to git:

#### **Excluded Directories**
- `bug_reports/` - Temporary bug documentation (periodically removed)
- `req_requests/` - Temporary requirement documents (periodically removed)
- `analysis/reports/` - Temporary analysis outputs
- `logs/` - Runtime logs and temporary data

#### **Excluded File Patterns**
- `*_REPORT.md` - Temporary reports
- `*_REQUIREMENTS.md` - Temporary requirements
- `*_BUG_*.md` - Bug report documents
- `*_MEGA_*.md` - Mega document outputs
- `debug_*.py` - Debug scripts (remove after use)
- `temp_*.py` - Temporary scripts (remove after use)
- `test_*.py` - Temporary test scripts (remove after use)

#### **Git Commit Rules**

**AI models MUST:**
1. **Never commit files in `bug_reports/` or `req_requests/` directories**
2. **Never commit temporary documentation files**
3. **Remove temporary files before committing**
4. **Only commit permanent code and the two allowed documentation files**

**Example of correct git workflow:**
```bash
# WRONG - includes temporary docs
git add bug_reports/ req_requests/ *.md

# RIGHT - only permanent code
git add algorithms/ models/ ui/ services/
git add docs/ARCHITECTURE.md docs/README.md  # Only these docs allowed
```

---

## 📄 Mega Document Generation Rules

**CRITICAL**: AI models must **NEVER** create mega documents unless explicitly instructed by the user.

**MANDATORY TOOL REQUIREMENT**: ALL mega documents MUST be created using `tools/create_mega_document.py`. Never create mega documents manually or through any other method.

### **When to Create Mega Documents**

**ONLY create mega documents when:**
- User explicitly requests a bug report
- User explicitly requests a requirement document
- User explicitly requests analysis documentation
- User specifically asks for comprehensive documentation

**NEVER create mega documents:**
- As part of completing a task
- To summarize work performed
- To document implementation details
- Without explicit user instruction

### **Mandatory Tool Usage Rule**

**ALL mega documents MUST be created using the enhanced create_mega_document.py tool:**

```bash
# REQUIRED: Use create_mega_document.py for ALL mega document creation
python tools/create_mega_document.py -f [files] -t "Title" -desc "Description" -o megadocs/DOCUMENT_NAME.md

# FORBIDDEN: Manual mega document creation
# ❌ NEVER manually write mega documents in text editors
# ❌ NEVER copy-paste large amounts of code manually
# ❌ NEVER create comprehensive documents without the tool
# ❌ NEVER bypass the create_mega_document.py tool
```

**Benefits of Mandatory Tool Usage:**
- **Consistent formatting** and structure across all mega documents
- **Automatic file validation** and error checking
- **Size management** with automatic splitting when needed
- **Complete source inclusion** with proper headers and metadata
- **Interactive file selection** for precise control
- **Professional presentation** with table of contents and organization

### **Mega Document Organization Policy (MANDATORY)**

#### **File Location Rules**
- **ALL mega documents MUST be placed in the `megadocs/` folder**
- **ALL mega documents MUST have proper `.md` file extensions**
- **NO mega documents allowed in project root or other directories**

#### **Naming Convention**
- **Format**: `[TOPIC]_[TYPE]_MEGADOC[_partN].md`
- **Examples**: 
  - `TFM_STRATEGY_BUG_REPORT_MEGADOC.md`
  - `BACKEND_PERFORMANCE_ANALYSIS_MEGADOC_part1.md`
  - `STRATEGY_ENHANCEMENT_REQUIREMENTS_MEGADOC.md`

#### **File Size Management**
- **Maximum size per part**: 300 KB
- **Split large documents**: Use `_part1.md`, `_part2.md`, etc.
- **Target total size**: 200-500 KB for focused analysis
- **File count limit**: 10-30 relevant files maximum per document

#### **Directory Structure Enforcement**
```
megadocs/
├── TFM_STRATEGY_BUG_REPORT_MEGADOC_part1.md
├── TFM_STRATEGY_BUG_REPORT_MEGADOC_part2.md
├── TFM_STRATEGY_BUG_REPORT_MEGADOC_part3.md
├── BACKEND_PERFORMANCE_ANALYSIS_MEGADOC.md
└── STRATEGY_ENHANCEMENT_REQUIREMENTS_MEGADOC.md
```

#### **Prohibited Locations**
- ❌ **Project root**: Never place mega docs in `/Users/yeogirlyun/C++/sentio_trader/`
- ❌ **Other directories**: Never place in `docs/`, `src/`, `include/`, etc.
- ❌ **Without extensions**: Never create files without `.md` extension
- ❌ **Temporary locations**: Never leave mega docs in temp directories

### **Mandatory Mega Document Creation Process**

**When explicitly requested, ALWAYS follow this exact process:**

#### **Step 1: Create Temporary Directory Structure**
```bash
# Create temporary directory for organizing relevant files
mkdir -p temp_mega_doc
```

#### **Step 2: Copy ONLY Relevant Source Modules**
```bash
# Copy ONLY the source modules directly related to the analysis
# Example for a backend bug analysis:
cp src/strategy_profiler.cpp temp_mega_doc/
cp src/adaptive_allocation_manager.cpp temp_mega_doc/
cp include/sentio/strategy_profiler.hpp temp_mega_doc/
cp include/sentio/adaptive_allocation_manager.hpp temp_mega_doc/
# ... only relevant files
```

#### **Step 3: Copy Bug Report or Requirement Document**
```bash
# Copy the bug report or requirement document to temp directory
cp BUG_REPORT_NAME.md temp_mega_doc/
# OR
cp REQUIREMENT_DOCUMENT_NAME.md temp_mega_doc/
```

#### **Step 4: Generate Mega Document**
```bash
# Use create_mega_document.py with the temp directory
python tools/create_mega_document.py \
  --directories temp_mega_doc \
  --title "Descriptive Title" \
  --description "Detailed description of the analysis" \
  --output megadocs/MEGA_DOCUMENT_NAME.md \
  --include-bug-report \
  --bug-report-file temp_mega_doc/BUG_REPORT_NAME.md
```

#### **Step 5: Clean Up Temporary Directory**
```bash
# Remove temporary directory after mega document creation
rm -rf temp_mega_doc
```

### **Critical Rules for File Selection**

**ONLY include files that are directly relevant to the analysis topic:**

#### ✅ **INCLUDE Files When Relevant**
- **Core Implementation**: Files directly implementing the analyzed functionality
- **Interface Headers**: Header files defining the interfaces being analyzed
- **Bug Report/Requirement**: The specific document that triggered the mega doc creation
- **Test Files**: Only tests directly related to the analyzed functionality
- **Configuration**: Only config files directly affecting the analyzed components

#### ❌ **EXCLUDE Files Always**
- **Unrelated Modules**: Files not connected to the analysis topic
- **Third-Party Libraries**: External dependencies and vendor code
- **Build Files**: Makefiles, CMake files, build scripts
- **Documentation**: General docs not specific to the analysis
- **Test Data**: Large data files, test datasets, sample files
- **Generated Files**: Auto-generated code, compiled binaries

### **Example Mega Document Creation Processes**

#### **Example 1: Backend Bug Analysis**
```bash
# Step 1: Create temp directory
mkdir -p temp_mega_doc

# Step 2: Copy only relevant backend files
cp src/strategy_profiler.cpp temp_mega_doc/
cp src/adaptive_allocation_manager.cpp temp_mega_doc/
cp src/universal_position_coordinator.cpp temp_mega_doc/
cp include/sentio/strategy_profiler.hpp temp_mega_doc/
cp include/sentio/adaptive_allocation_manager.hpp temp_mega_doc/
cp include/sentio/universal_position_coordinator.hpp temp_mega_doc/

# Step 3: Copy bug report
cp BACKEND_BUG_REPORT.md temp_mega_doc/

# Step 4: Generate mega document
python tools/create_mega_document.py \
  --directories temp_mega_doc \
  --title "Backend Critical Bug Analysis" \
  --description "Analysis of strategy-agnostic backend issues" \
  --output megadocs/BACKEND_BUG_ANALYSIS_MEGA_DOC.md \
  --include-bug-report \
  --bug-report-file temp_mega_doc/BACKEND_BUG_REPORT.md

# Step 5: Clean up
rm -rf temp_mega_doc
```

#### **Example 2: Strategy Enhancement Request**
```bash
# Step 1: Create temp directory
mkdir -p temp_mega_doc

# Step 2: Copy only relevant strategy files
cp src/strategy_signal_or.cpp temp_mega_doc/
cp include/sentio/strategy_signal_or.hpp temp_mega_doc/
cp include/sentio/detectors/rsi_detector.hpp temp_mega_doc/
cp include/sentio/detectors/bollinger_detector.hpp temp_mega_doc/

# Step 3: Copy requirement document
cp STRATEGY_ENHANCEMENT_REQUIREMENTS.md temp_mega_doc/

# Step 4: Generate mega document
python tools/create_mega_document.py \
  --directories temp_mega_doc \
  --title "Strategy Enhancement Analysis" \
  --description "Analysis for new strategy detector integration" \
  --output megadocs/STRATEGY_ENHANCEMENT_MEGA_DOC.md \
  --include-bug-report \
  --bug-report-file temp_mega_doc/STRATEGY_ENHANCEMENT_REQUIREMENTS.md

# Step 5: Clean up
rm -rf temp_mega_doc
```


### **Size Guidelines and Best Practices**

#### **Target Specifications**
- **Target Size**: 200-500 KB maximum for focused analysis
- **File Count**: 5-15 relevant files maximum (not 182 files!)
- **Content Focus**: Only files directly related to the analysis topic

#### **Benefits of Using the Temporary Directory Process**

1. **Focused Analysis**: Only relevant files included, not entire codebase
2. **Manageable Size**: Documents stay under 500 KB for efficient processing
3. **Clean Organization**: Temporary structure keeps project clean
4. **Comprehensive Context**: Bug report + relevant source code together
5. **AI-Optimized**: Right amount of context without information overload

### **Prohibited Actions**

❌ **NEVER DO**: Create mega documents without explicit user request
❌ **NEVER DO**: Create mega documents manually in chat or text editors
❌ **NEVER DO**: Copy/paste large amounts of source code directly  
❌ **NEVER DO**: Create comprehensive documents without using create_mega_document.py tool
❌ **NEVER DO**: Write documents >1KB manually in responses
❌ **NEVER DO**: Update existing bug reports unless explicitly instructed
❌ **NEVER DO**: Create summary or completion documents
❌ **NEVER DO**: Include entire directories (src/, include/) without filtering
❌ **NEVER DO**: Skip the temporary directory process
❌ **NEVER DO**: Place mega documents outside `megadocs/` folder
❌ **NEVER DO**: Create mega documents without `.md` extensions
❌ **NEVER DO**: Leave mega documents in project root or temp directories
❌ **NEVER DO**: Bypass create_mega_document.py tool for any mega document creation

✅ **ONLY DO WHEN EXPLICITLY REQUESTED**: Use the 5-step temporary directory process
✅ **WHEN REQUESTED**: Copy ONLY relevant source modules to temp directory
✅ **WHEN REQUESTED**: Include the bug report or requirement document
✅ **WHEN REQUESTED**: Use `create_mega_document.py` with `--directories temp_mega_doc`
✅ **WHEN REQUESTED**: Clean up temporary directory after creation
✅ **MANDATORY**: Always output to `megadocs/` folder with `.md` extension
✅ **MANDATORY**: Use proper naming convention `[TOPIC]_[TYPE]_MEGADOC[_partN].md`
✅ **MANDATORY**: Verify file sizes stay under 300KB per part

### **Common Use Cases**

- **Bug Analysis**: Performance issues, algorithm failures, integration problems
- **Feature Requests**: New algorithm implementations, UI enhancements, system upgrades  
- **Performance Reviews**: Benchmarking, optimization analysis, comparison studies
- **Architecture Analysis**: System design reviews, component integration studies
- **Code Reviews**: Comprehensive code analysis with multiple file context

### **Mega Document Size Management (CRITICAL)**

**MANDATORY**: When creating mega documents, include **ONLY relevant source modules** to maintain manageable document sizes.

#### **Size Guidelines**
- **Target Size**: 200-500 KB maximum for focused analysis
- **File Count**: 10-30 relevant files maximum
- **Content Focus**: Only include files directly related to the analysis topic

#### **File Selection Rules**

##### ✅ **INCLUDE Files When Relevant**
- **Core Implementation**: Files directly implementing the analyzed functionality
- **Interface Definitions**: Headers defining the analyzed components
- **Supporting Infrastructure**: Files that directly support the core functionality
- **Configuration**: Files that configure the analyzed behavior
- **Test Files**: Files that test the analyzed functionality

##### ❌ **EXCLUDE Files When Unrelated**
- **Strategy Implementations**: Unless analyzing strategy-specific issues
- **Feature Engineering**: Unless analyzing feature-specific problems
- **ML Models**: Unless analyzing model-specific issues
- **Portfolio Management**: Unless analyzing portfolio-specific problems
- **Signal Processing**: Unless analyzing signal-specific issues
- **UI Components**: Unless analyzing UI-specific problems
- **Utility Functions**: Unless directly related to the analysis

#### **Relevance Assessment Criteria**

**A file is RELEVANT if:**
1. **Direct Implementation**: Contains code that directly implements the analyzed functionality
2. **Core Interface**: Defines interfaces used by the analyzed functionality
3. **Configuration**: Contains settings that affect the analyzed behavior
4. **Supporting Logic**: Contains logic that directly supports the analyzed functionality
5. **Testing**: Contains tests that validate the analyzed functionality

**A file is UNRELATED if:**
1. **Different Domain**: Implements functionality in a different domain (e.g., UI when analyzing backend)
2. **Indirect Support**: Only indirectly supports the analyzed functionality
3. **Generic Utility**: Provides generic functionality not specific to the analysis
4. **Strategy-Specific**: Implements specific strategies when analyzing general framework issues
5. **Feature-Specific**: Implements specific features when analyzing core system issues

#### **Size Management Examples**

##### **Example 1: TPA Metrics Analysis**
**RELEVANT (15-20 files):**
- `include/sentio/metrics.hpp` - Core metrics calculation
- `src/temporal_analysis.cpp` - TPA implementation
- `src/runner.cpp` - Backtest execution
- `audit/src/audit_db.cpp` - Audit metrics calculation
- `audit/src/audit_cli.cpp` - Audit reporting
- `include/sentio/runner.hpp` - Runner interface
- `include/sentio/temporal_analysis.hpp` - TPA interface
- `include/sentio/base_strategy.hpp` - Strategy base class
- `src/audit.cpp` - Audit recorder
- `src/audit_validator.cpp` - Audit validation

**UNRELATED (100+ files):**
- All strategy implementations (`strategy_*.cpp`)
- Feature engineering modules (`feature_engineering/`)
- ML model implementations (`ml/`)
- Portfolio management (`portfolio/`)
- Signal processing (`signal_*.cpp`)

##### **Example 2: Strategy-Specific Bug**
**RELEVANT (5-10 files):**
- `src/strategy_ire.cpp` - Specific strategy implementation
- `include/sentio/strategy_ire.hpp` - Strategy interface
- `include/sentio/base_strategy.hpp` - Base strategy class
- `src/runner.cpp` - Strategy execution
- `include/sentio/runner.hpp` - Runner interface

**UNRELATED (150+ files):**
- All other strategy implementations
- Audit system files
- Feature engineering modules
- ML model implementations
- Portfolio management files

#### **Pre-Creation Checklist**

Before creating a mega document, verify:
- [ ] **File Relevance**: Each included file directly relates to the analysis topic
- [ ] **Size Estimate**: Document will be <500 KB
- [ ] **File Count**: <30 files included
- [ ] **Domain Focus**: Files are from the same functional domain
- [ ] **Analysis Scope**: Files match the analysis scope and depth

#### **Size Monitoring Commands**

```bash
# Check mega document size before creation
ls -lh megadocs/*.md | awk '{print $5, $9}' | sort -hr

# Count files in mega document
grep "^## 📄 \*\*FILE.*\*\*:" megadocs/DOCUMENT_NAME.md | wc -l

# List files included in mega document
grep "^## 📄 \*\*FILE.*\*\*:" megadocs/DOCUMENT_NAME.md | sed 's/## 📄 \*\*FILE [0-9]* of [0-9]*\*\*: //'
```

### **Enhanced Tool Command Reference**

The enhanced `create_mega_document.py` supports multiple input methods and advanced features:

```bash
# METHOD 1: Directory-based (with auto-splitting)
python tools/create_mega_document.py \
  --directories src/training src/strategy \
  --title "Component Analysis" \
  --description "Analysis of training and strategy components" \
  --output megadocs/COMPONENT_ANALYSIS_MEGADOC.md \
  --max-size 300

# METHOD 2: Specific file list (recommended for focused analysis)
python tools/create_mega_document.py \
  --files src/cli/preprocess_data.cpp src/cli/tfm_trainer_efficient.cpp include/training/quality_enforced_loss.h \
  --title "Data Preprocessing Bug Analysis" \
  --description "Analysis of preprocessing quality issues" \
  --output megadocs/PREPROCESSING_BUG_MEGADOC.md \
  --single-doc

# METHOD 3: Interactive file selection (for complex filtering)
python tools/create_mega_document.py \
  --directories src/ include/ \
  --title "Interactive Selection" \
  --description "User-selected files for analysis" \
  --output megadocs/SELECTED_FILES_MEGADOC.md \
  --interactive

# METHOD 4: With bug report integration
python tools/create_mega_document.py \
  --files relevant_file1.cpp relevant_file2.h \
  --title "Bug Analysis with Report" \
  --description "Complete bug analysis with source code" \
  --output megadocs/BUG_ANALYSIS_MEGADOC.md \
  --include-bug-report \
  --bug-report-file BUG_REPORT.md

# Advanced Options:
# --single-doc     : Create one document without size splitting
# --max-size N     : Custom size limit per part (default: 300KB)
# --interactive    : Interactive file selection with filtering
# --verbose        : Detailed output for debugging

# CRITICAL: Output path MUST be in megadocs/ folder
# CRITICAL: Output filename MUST end with .md extension
# CRITICAL: Tool automatically creates _part1.md, _part2.md, etc. for large documents
```

### **Input Method Selection Guide**

| Use Case | Recommended Method | Example |
|----------|-------------------|---------|
| **Focused bug analysis** | `--files` with specific files | Preprocessing quality issues |
| **Component exploration** | `--directories` with filtering | Training system analysis |
| **Complex selection** | `--interactive` mode | User-guided file selection |
| **Large codebase** | `--directories` with size limits | System-wide analysis |
| **With bug reports** | Any method + `--include-bug-report` | Complete analysis with context |

### **Post-Creation Verification**

After creating mega documents, verify:
```bash
# Check all mega documents are in correct location
ls -la megadocs/*.md

# Verify file sizes are under 300KB per part
ls -lh megadocs/*.md | awk '{print $5, $9}' | sort -hr

# Confirm proper naming convention
ls megadocs/ | grep -E ".*_MEGADOC.*\.md$"
```

---

## 🔧 Development Workflow

### **Standard Development Process**

1. **Code Changes**: Implement features following architecture rules
2. **Integration**: Ensure GUI and multi-algorithm integration
3. **Testing**: Verify functionality and performance
4. **Documentation**: Update `docs/ARCHITECTURE.md` and `docs/README.md`
5. **Cleanup**: Remove any temporary files or outdated information

### **Feature Addition Checklist**

- [ ] Integrates with multi-algorithm system
- [ ] Has GUI controls and monitoring
- [ ] Includes performance tracking
- [ ] Uses Kafka messaging appropriately
- [ ] Follows error handling patterns
- [ ] Updates `docs/ARCHITECTURE.md`
- [ ] Updates `docs/README.md`
- [ ] No new documentation files created

### **Bug Fix Checklist**

- [ ] Identifies root cause
- [ ] Implements proper fix
- [ ] Tests fix thoroughly
- [ ] Updates documentation if architecture affected
- [ ] No temporary documentation created

---

## 🎯 Quality Standards

### **Code Quality**

1. **Performance**: Sub-second response times for GUI operations
2. **Reliability**: Graceful error handling and recovery
3. **Scalability**: Support for multiple algorithms and symbols
4. **Maintainability**: Clear, documented code structure
5. **Integration**: Seamless component interaction

### **Logging Policy (Mandatory)**

All components must use centralized, structured JSON logging:

1. Initialize once via `core.json_logging.configure_json_logging()` at process start.
2. Do not use `print()` in production code. Use `logging.getLogger(__name__)` only.
3. JSON fields emitted by default: `timestamp`, `level`, `logger`, `message`, `run_id`.
4. Include when available: `algo`, `symbol`, `order_id`, `event_seq`, `event`, `component`.
5. Sinks: stdout and `logs/app.jsonl`. Errors also recorded in `logs/errors.log`.
6. Domain messages (Kafka/persistence) must carry `run_id` and `event_seq`.
7. UI and background threads must not directly mutate widgets; emit events/logs instead.

### **Documentation Quality**

1. **Accuracy**: Documentation matches current codebase exactly
2. **Completeness**: All features and components documented
3. **Clarity**: Clear instructions and explanations
4. **Currency**: No outdated information
5. **Consolidation**: All information in two files only

### **User Experience**

1. **Intuitive GUI**: Easy-to-use interface
2. **Real-time Feedback**: Live performance monitoring
3. **Professional Appearance**: Consistent theme system
4. **Reliable Operation**: Stable, predictable behavior
5. **Clear Documentation**: Easy setup and usage instructions

---

## 🧭 UI Design Principles (Global)

- **Grid, alignment, and spacing**
  - Use a consistent grid (e.g., 8pt) with aligned gutters; avoid wide, unallocated columns
  - Keep vertical rhythm consistent; don’t leave single giant blocks surrounded by empty space

- **Information density controls**
  - Provide density modes (compact/comfortable) for tables and lists
  - Balance whitespace: enough for scanability, not so much that primary content gets crowded

- **Container‑aware sizing**
  - Constrain components with sensible min/max sizes
  - Avoid full‑width for low‑content widgets; prefer fit‑to‑content unless the content benefits from “fill available” (e.g., text areas, charts)

- **Visual balance and hierarchy**
  - Use typographic scale, contrast, and proximity to structure content
  - Place secondary metrics/actions in otherwise empty zones only when it improves workflows

- **Content‑first layout**
  - Prioritize the information users read/act on most
  - Allocate area proportionally to content importance and frequency of use


## 🚨 Enforcement

### **Automatic Checks**

AI models should verify:
- No new `.md` files created
- Both documentation files updated when needed
- Architecture changes reflected in documentation
- User-facing changes reflected in README

### **Review Requirements**

Before completing any work:
1. **Architecture Review**: Changes match documented architecture
2. **Documentation Review**: Both files are current and accurate
3. **Integration Review**: Components work together properly
4. **Performance Review**: System meets performance standards

---

## 📈 Success Metrics

### **Documentation Success**
- **Single Source of Truth**: All information in two files
- **Always Current**: Documentation matches codebase
- **User-Friendly**: Clear installation and usage instructions
- **Technically Complete**: Full architecture documentation

### **System Success**
- **Multi-Algorithm Performance**: All algorithms integrated and performing
- **GUI Functionality**: Complete control and monitoring interface
- **Real-time Operation**: Live trading and performance tracking
- **Professional Quality**: Institutional-grade trading platform

---

## 🎉 Conclusion

These rules ensure Sentio Trader maintains:
- **Clean Documentation**: Two comprehensive, current files
- **Professional Architecture**: Consistent, scalable system design
- **Quality Standards**: Reliable, high-performance operation
- **User Experience**: Clear, intuitive interface and documentation
- **Controlled Documentation**: No unsolicited document creation

### **KEY POLICY SUMMARY**

#### **Document Creation Policy**
- **Source Code**: ✅ Create as needed to implement user instructions
- **Configuration Files**: ✅ Create as needed for functionality  
- **Two Permanent Docs**: ✅ Update `docs/ARCHITECTURE.md` and `docs/README.md` only if needed
- **All Other Documentation**: ❌ NEVER create unless user explicitly requests
- **Bug Reports**: ❌ NEVER create unless user explicitly requests
- **Analysis Documents**: ❌ NEVER create unless user explicitly requests
- **Summary Reports**: ❌ NEVER create (provide brief chat summaries instead)
- **Implementation Guides**: ❌ NEVER create (update readme.md instead)
- **Task Completion Reports**: ❌ NEVER create (brief chat summary instead)

#### **Default AI Behavior Protocol**
1. **Complete the requested task** with source code changes
2. **Update permanent docs ONLY if needed** (`docs/ARCHITECTURE.md` for architecture changes, `docs/README.md` for user-facing changes)
3. **Provide brief summary in chat** - Never create summary documents
4. **Clean up temporary files** created during implementation  
5. **STOP** - Do not create any documentation files unless user explicitly requests

**All AI models and contributors must follow these rules without exception.**

## 🏗️ **Architecture Consolidation Rules (Added 2025-09-02)**

- **UNIFIED SYSTEM**: Use `sentio_exec` for all new strategy development, testing, and production deployment
- **DEPRECATED MODULE**: `sentio_unified` is DEPRECATED - do not use for new development  
- **CONSOLIDATION RATIONALE**: Eliminates duplicate classes, provides unified Trade Manager architecture, maintains zero-tolerance for code duplication
- **MIGRATION PATH**: All `sentio_unified` functionality consolidated into `sentio_exec` with enhanced features

---

*Sentio Trader Project Rules - Ensuring Quality and Consistency*
# LeveragedPositionManager Requirements Documentation Rule

When creating comprehensive documentation with multiple source files (like requirements documents with implementation references), use create_mega_document.py to generate consolidated megadocs in the megadocs folder. This ensures proper organization, indexing, and version tracking of complex technical documentation.

# Bug Report Documentation Rule

When creating bug reports for complex issues involving multiple components, use create_mega_document.py to generate comprehensive megadocs that include:
- Detailed bug analysis with root cause investigation
- All relevant source modules and implementation files  
- Performance metrics and comparison data
- Recommended solutions with implementation priority
- Complete source code for review and analysis

This ensures thorough documentation and enables effective debugging and resolution of complex system issues.


# Streamlined Architecture Update

The Sentio Trader codebase has been streamlined to focus on three core trading strategies:
- SGO (Sigor): Proven non-AI strategy with 56.5% accuracy  
- XGBoost: Gradient boosting ML approach (currently under development)
- PPO: Proximal Policy Optimization reinforcement learning

Removed components for simplification:
- TFM (Transformer): Complex neural network approach with overfitting issues
- GRU: Recurrent neural network implementation
- Related training tools, artifacts, and documentation

This streamlining reduces complexity while maintaining the most promising approaches for production trading.

---

## 🚫 **CRITICAL NO-FALLBACK RULE: Fundamental Solutions Only**

### **Mandatory Solution Standard**
**NO fallback methods, workarounds, or temporary solutions are allowed. All problems must be solved at the fundamental level.**

#### **🎯 RULE: Fix Root Causes, Not Symptoms**
- **Fundamental Fix Required**: Every problem must be solved by addressing its root cause
- **No Workarounds**: Temporary solutions, fallbacks, and workarounds are forbidden
- **No "Good Enough"**: Partial solutions that leave underlying issues unresolved are not acceptable
- **Complete Resolution**: Problems must be fully resolved, not worked around

#### **❌ FORBIDDEN Approaches**
- **Fallback Methods**: "If X fails, do Y instead" - Fix X properly
- **Workarounds**: "We'll bypass this issue by..." - Solve the issue directly
- **Temporary Solutions**: "For now, we'll..." - Implement the permanent solution
- **Partial Fixes**: "This mostly works except..." - Make it work completely
- **Compromise Solutions**: "It's not perfect but..." - Make it perfect

#### **✅ REQUIRED Approaches**
- **Root Cause Analysis**: Identify and fix the fundamental problem
- **Complete Solutions**: Address all aspects of the issue
- **Proper Integration**: Ensure components work together correctly
- **Thorough Testing**: Verify the fundamental fix works in all scenarios
- **No Shortcuts**: Take the time to implement the right solution

#### **🔧 Implementation Examples**

##### **❌ WRONG: Fallback Approach**
```cpp
try {
    // Try BackendComponent
    auto backend = std::make_unique<BackendComponent>(config);
    return backend->process_to_jsonl(...);
} catch (const std::exception& e) {
    // FALLBACK: Use simplified manual processing
    return generate_simplified_trades(...);
}
```

##### **✅ RIGHT: Fundamental Fix**
```cpp
// Fix the BackendComponent integration properly
// Debug initialization issues, fix dependencies, resolve crashes
// Ensure BackendComponent works correctly in all scenarios
auto backend = std::make_unique<BackendComponent>(config);
return backend->process_to_jsonl(...);
// No fallbacks - if it fails, fix the fundamental issue
```

#### **🚨 Enforcement Protocol**
When encountering problems:
1. **STOP**: Do not implement workarounds or fallbacks
2. **ANALYZE**: Identify the root cause of the problem
3. **DEBUG**: Investigate why the proper solution isn't working
4. **FIX**: Implement the fundamental solution
5. **TEST**: Verify the fix works completely
6. **DOCUMENT**: Update architecture to reflect the proper solution

#### **📋 Problem-Solving Checklist**
Before implementing any solution, verify:
- [ ] **Root Cause Identified**: The fundamental problem is understood
- [ ] **Proper Solution Designed**: The fix addresses the root cause
- [ ] **No Workarounds**: The solution doesn't bypass any issues
- [ ] **Complete Resolution**: All aspects of the problem are solved
- [ ] **Integration Verified**: The solution works with existing systems
- [ ] **Testing Complete**: The fix works in all scenarios

#### **🎯 Benefits of Fundamental Solutions**
1. **Long-term Stability**: Problems are solved permanently
2. **System Integrity**: All components work as designed
3. **Maintainability**: No complex workaround logic to maintain
4. **Reliability**: Systems work correctly under all conditions
5. **Professional Quality**: Solutions meet production standards

#### **📚 Real-World Application**
**Current Example: BackendComponent Integration**
- **Problem**: BackendComponent crashes during audit processing
- **WRONG Approach**: Implement fallback to artificial trade generation
- **RIGHT Approach**: Debug and fix BackendComponent initialization and dependencies
- **Result**: Proper integration with realistic trade processing

**This rule ensures all solutions are production-ready and maintainable.**

---

## ⚠️ **CRITICAL FINANCIAL SYSTEM RULE: NO FALLBACK MECHANISMS**

### **Mandatory Safety Standard for Real Money Trading**
**SENTIO USES REAL MONEY FOR TRADING. Fallback mechanisms are FORBIDDEN as they mask bugs and produce incorrect results that could lead to financial losses.**

#### **🎯 RULE: Fail Loudly, Fail Fast, Fix Fundamentally**
- **NO Fallback Logic**: Never catch exceptions and return approximate/fallback results
- **NO Silent Failures**: Never suppress errors and continue with degraded functionality  
- **NO "Good Enough" Calculations**: Never use simplified calculations when the proper method fails
- **CRASH with Details**: When something fails, crash immediately with comprehensive error messages
- **FIX Before Proceeding**: Do not allow trading to continue with buggy calculations

#### **❌ FORBIDDEN: Fallback Mechanisms**
```cpp
// ❌ ABSOLUTELY FORBIDDEN IN FINANCIAL SYSTEMS
try {
    double mrb = calculate_accurate_mrb(...);
    return mrb;
} catch (const std::exception& e) {
    // FALLBACK: Use approximate calculation
    return calculate_approximate_mrb(...);  // ← DANGEROUS!
}
```

**Why This is Dangerous:**
- **Masks Bugs**: The real problem (why accurate calculation failed) is hidden
- **Wrong Results**: Approximate calculations may be completely wrong
- **Silent Corruption**: Trading continues with incorrect risk/return assessments
- **Money at Risk**: Decisions based on wrong calculations = real financial losses

#### **✅ REQUIRED: Fail Fast with Details**
```cpp
// ✅ CORRECT: Crash immediately with diagnostic information
try {
    double mrb = calculate_accurate_mrb(...);
    return mrb;
} catch (const std::exception& e) {
    std::cerr << "\n";
    std::cerr << "╔════════════════════════════════════════════════════════╗\n";
    std::cerr << "║  CRITICAL ERROR: MRB Calculation Failed                ║\n";
    std::cerr << "╚════════════════════════════════════════════════════════╝\n";
    std::cerr << "\n";
    std::cerr << "Exception: " << e.what() << "\n";
    std::cerr << "Context: [detailed context here]\n";
    std::cerr << "\n";
    std::cerr << "⚠️  Sentio uses REAL MONEY for trading.\n";
    std::cerr << "⚠️  Fallback mechanisms are DISABLED.\n";
    std::cerr << "⚠️  Fix the underlying issue before proceeding.\n";
    std::cerr << "\n";
    
    // RE-THROW - Do NOT continue with bad data
    throw std::runtime_error("MRB calculation failed: " + std::string(e.what()));
}
```

**Why This is Correct:**
- **Exposes Bugs**: Forces immediate investigation of the real problem
- **Prevents Loss**: Stops trading before wrong decisions are made
- **Clear Diagnostics**: Provides all information needed to debug the issue
- **Safe Operation**: No trading with corrupted/incorrect data

#### **🚨 CRITICAL EXAMPLES**

##### **Example 1: MRB Calculation**
**WRONG (FORBIDDEN):**
```cpp
double mrb = calculate_with_enhanced_psm(signals, data);
if (mrb == 0.0) {
    // FALLBACK: Use simple calculation
    mrb = calculate_simple_mrb(signals, data);  // ← Masks the bug!
}
```

**RIGHT (REQUIRED):**
```cpp
double mrb = calculate_with_enhanced_psm(signals, data);
// If calculation fails, exception is thrown and propagated
// System crashes with detailed error - operator MUST fix before trading
```

##### **Example 2: Signal Generation**
**WRONG (FORBIDDEN):**
```cpp
try {
    signals = strategy->generate_signals(data);
} catch (...) {
    // FALLBACK: Use default neutral signals
    signals = generate_neutral_signals(data);  // ← Silent failure!
}
```

**RIGHT (REQUIRED):**
```cpp
signals = strategy->generate_signals(data);
// If generation fails, let exception propagate
// System stops, error is investigated and fixed
```

##### **Example 3: Price Lookup**
**WRONG (FORBIDDEN):**
```cpp
double price = get_market_price(symbol, timestamp);
if (price == 0.0) {
    // FALLBACK: Use approximate price
    price = 270.0;  // ← Hardcoded fallback masks data bug!
}
```

**RIGHT (REQUIRED):**
```cpp
double price = get_market_price(symbol, timestamp);
if (price <= 0.0) {
    throw std::runtime_error(
        "Invalid price for " + symbol + " at " + 
        std::to_string(timestamp) + ": " + std::to_string(price)
    );
}
// Only proceed with valid, verified price
```

#### **📋 Code Review Checklist**

Before merging any code, verify:
- [ ] **NO try-catch with fallback return values**
- [ ] **NO default/approximate calculations when primary fails**
- [ ] **NO silent error suppression**
- [ ] **All failures propagate with detailed messages**
- [ ] **System stops immediately on calculation errors**
- [ ] **No trading continues with degraded/uncertain data**

#### **🎯 Rationale: Financial System Integrity**

**In financial systems, it is ALWAYS better to:**
- **Stop trading** than trade with wrong calculations
- **Investigate errors** than mask them with fallbacks
- **Fix bugs immediately** than work around them
- **Lose one day of trading** than lose money on bad decisions

**Key Principle:**
> **"If you don't trust the calculation enough to trade on it, 
> don't return an approximate value - CRASH and FIX IT."**

#### **🔧 Error Handling Standards**

**DO:**
- ✅ Throw exceptions with detailed context
- ✅ Log comprehensive error information
- ✅ Stop execution immediately
- ✅ Provide actionable debugging information
- ✅ Re-throw exceptions to propagate failures

**DON'T:**
- ❌ Catch and return default/fallback values
- ❌ Log errors but continue with bad data
- ❌ Use approximate calculations silently
- ❌ Suppress exceptions without re-throwing
- ❌ Continue trading with uncertain results

#### **🚨 Enforcement: Zero Tolerance**

**This rule has ZERO TOLERANCE for violations:**
- Any code with fallback mechanisms will be **REJECTED**
- Any silent error handling will be **REJECTED**
- Any approximate calculations after failures will be **REJECTED**
- Any code that continues after calculation errors will be **REJECTED**

**ALL AI models and contributors must follow this rule without exception when working on Sentio Trader.**

---

## 🎯 **ONLINE TRADER PROJECT SUMMARY**

### **Key Focus Areas**
- **Online Learning Algorithms**: Real-time adaptation with incremental updates
- **Ensemble PSM**: Multi-strategy coordination with dynamic weighting
- **Strategy Framework**: Modular OnlineStrategyBase interface
- **Testing Framework**: Comprehensive online learning validation
- **CLI Interface**: Complete command-line testing suite

### **Removed Components (By Design)**
- ❌ XGBoost strategies (offline ML)
- ❌ CatBoost strategies (offline ML)  
- ❌ PPO/RL strategies (separate focus)
- ❌ TFT/Transformer strategies (separate focus)
- ❌ SGO meta-strategies (offline ML ensemble)

### **Core Dependencies**
- ✅ **Eigen3** (required for online learning)
- ✅ **nlohmann/json** (optional)
- ✅ **OpenMP** (optional, for performance)

### **Documentation Files**
1. **README.md** - Project overview and usage
2. **QUICKSTART.md** - Quick start guide
3. **TESTING_GUIDE.md** - Testing framework documentation
4. **PROJECT_STATUS.md** - Current project status
5. **MIGRATION_SUMMARY.md** - Migration from sentio_trader

### **Key Executables**
- **sentio_cli** - Main CLI for online learning commands
- **test_online_trade** - Online learning test tool
- **csv_to_binary_converter** - Data format conversion

### **Success Metrics**
- **Zero** unauthorized testing modules created
- **100%** testing tasks solved with online testing framework
- **Real-time adaptation** maintained for all strategies
- **Ensemble coordination** for multi-strategy scenarios
- **Position integrity** preserved (no negative/conflicting positions)

---

**Remember: Online Trader focuses on REAL-TIME ADAPTATION. Better to crash than to silently trade with outdated models.**
