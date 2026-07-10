#pragma once
#include "FUCK_API.h"

namespace FUCK::Sugar {
using ScopeEndCallback = void (*)();

// RAII scope guard for FUCK Begin* functions returning bool.
template <bool AlwaysCallEnd>
struct BooleanGuard {
  BooleanGuard(const bool state, const ScopeEndCallback end) noexcept
      : m_state(state), m_end(end) {}

  BooleanGuard(const BooleanGuard<AlwaysCallEnd>&) = delete;
  BooleanGuard(BooleanGuard<AlwaysCallEnd>&&) = delete;
  BooleanGuard<AlwaysCallEnd>& operator=(const BooleanGuard<AlwaysCallEnd>&) =
      delete;  // NOLINT
  BooleanGuard<AlwaysCallEnd>& operator=(BooleanGuard<AlwaysCallEnd>&&) =
      delete;  // NOLINT

  ~BooleanGuard() noexcept;

  operator bool() const& noexcept { return m_state; }  // (Implicit) NOLINT

 private:
  const bool m_state;
  const ScopeEndCallback m_end;
};

template <>
inline BooleanGuard<true>::~BooleanGuard() noexcept {
  m_end();
}

template <>
inline BooleanGuard<false>::~BooleanGuard() noexcept {
  if (m_state) {
    m_end();
  }
}

// For special cases, transform void(*)(int) to void(*)()
inline void PopStyleColor() { FUCK::PopStyleColor(1); };
inline void PopStyleVar() { FUCK::PopStyleVar(1); };
inline void Unindent() { FUCK::Unindent(); }

// Tooltip auto triggered on hover
inline auto BeginTooltip() -> bool {
  if (FUCK::IsItemHovered()) {
    ImGui::BeginTooltip();
    return true;
  }
  return false;
}

}  // namespace FUCK::Sugar

// ----------------------------------------------------------------------------
// [SECTION] Utility macros
// ----------------------------------------------------------------------------

// Portable Expression Statement, calls void function and returns true
#define FUCK_SUGAR_ES(FN, ...) \
  ([&]() -> bool {              \
    FN(__VA_ARGS__);            \
    return true;                \
  }())
#define FUCK_SUGAR_ES_0(FN) \
  ([&]() -> bool {           \
    FN();                    \
    return true;             \
  }())

// Concatenating symbols with __LINE__ requires two levels of indirection
#define FUCK_SUGAR_CONCAT0(A, B) A##B
#define FUCK_SUGAR_CONCAT1(A, B) FUCK_SUGAR_CONCAT0(A, B)

// ----------------------------------------------------------------------------
// [SECTION] Generic macros to simplify repetitive declarations
// ----------------------------------------------------------------------------
//
// +----------------------+-------------------+-----------------+---------------------+
// | BEGIN                | END               | ALWAYS          | __VA_ARGS__ |
// +----------------------+-------------------+-----------------+---------------------+
// | Begin*/Push*         | End*/Pop*         | Is call to END  | Begin*/Push* |
// | function name        | function name     | unconditional?  | function
// arguments  |
// +----------------------+-------------------+-----------------+---------------------+

#define FUCK_SUGAR_SCOPED_BOOL(BEGIN, END, ALWAYS, ...)            \
  if (const FUCK::Sugar::BooleanGuard<ALWAYS> FUCK_SUGAR_CONCAT1( \
          _ui_scope_guard, __LINE__) = {BEGIN(__VA_ARGS__), &END})

#define FUCK_SUGAR_SCOPED_BOOL_0(BEGIN, END, ALWAYS)               \
  if (const FUCK::Sugar::BooleanGuard<ALWAYS> FUCK_SUGAR_CONCAT1( \
          _ui_scope_guard, __LINE__) = {BEGIN(), &END})

#define FUCK_SUGAR_SCOPED_VOID_N(BEGIN, END, ...)                          \
  if (const FUCK::Sugar::BooleanGuard<true> FUCK_SUGAR_CONCAT1(           \
          _ui_scope_guard, __LINE__) = {FUCK_SUGAR_ES(BEGIN, __VA_ARGS__), \
                                        &END})

#define FUCK_SUGAR_SCOPED_VOID_0(BEGIN, END)                     \
  if (const FUCK::Sugar::BooleanGuard<true> FUCK_SUGAR_CONCAT1( \
          _ui_scope_guard, __LINE__) = {FUCK_SUGAR_ES_0(BEGIN), &END})

#define FUCK_SUGAR_PARENT_SCOPED_VOID_N(BEGIN, END, ...)     \
  const FUCK::Sugar::BooleanGuard<true> FUCK_SUGAR_CONCAT1( \
      _ui_scope_, __LINE__) = {FUCK_SUGAR_ES(BEGIN, __VA_ARGS__), &END}

// ---------------------------------------------------------------------------
// [SECTION] FUCK DSL
// ----------------------------------------------------------------------------

#define FUCK_Window(...) \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::Begin, ImGui::End, true, __VA_ARGS__)
#define FUCK_Child(...) \
  FUCK_SUGAR_SCOPED_BOOL(FUCK::BeginChild, FUCK::EndChild, true, __VA_ARGS__)
#define FUCK_ChildFrame(...)                                                 \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::BeginChildFrame, ImGui::EndChildFrame, true, \
                          __VA_ARGS__)
#define FUCK_Combo(...)                                             \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::BeginCombo, ImGui::EndCombo, false, \
                          __VA_ARGS__)
#define FUCK_ListBox(...)                                               \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::BeginListBox, ImGui::EndListBox, false, \
                          __VA_ARGS__)
#define FUCK_Menu(...) \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::BeginMenu, FUCK::EndMenu, false, __VA_ARGS__)
#define FUCK_Popup(...)                                             \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::BeginPopup, FUCK::EndPopup, false, \
                          __VA_ARGS__)
#define FUCK_PopupModal(...)                                             \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::BeginPopupModal, FUCK::EndPopup, false, \
                          __VA_ARGS__)
#define FUCK_PopupContextItem(...)                                      \
  FUCK_SUGAR_SCOPED_BOOL(FUCK::BeginPopupContextItem, FUCK::EndPopup, \
                          false, __VA_ARGS__)
#define FUCK_PopupContextWindow(...)                                      \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::BeginPopupContextWindow, FUCK::EndPopup, \
                          false, __VA_ARGS__)
#define FUCK_PopupContextVoid(...)                                      \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::BeginPopupContextVoid, FUCK::EndPopup, \
                          false, __VA_ARGS__)
#define FUCK_Table(...)                                             \
  FUCK_SUGAR_SCOPED_BOOL(FUCK::BeginTable, FUCK::EndTable, false, \
                          __VA_ARGS__)
#define FUCK_TabBar(...)                                              \
  FUCK_SUGAR_SCOPED_BOOL(FUCK::BeginTabBar, FUCK::EndTabBar, false, \
                          __VA_ARGS__)
#define FUCK_TabItem(...)                             \
  FUCK_SUGAR_SCOPED_BOOL(FUCK::BeginTabItem, FUCK::EndTabItem, false, \
                          __VA_ARGS__)
#define FUCK_TreeNode(...) \
  FUCK_SUGAR_SCOPED_BOOL(FUCK::TreeNode, FUCK::TreePop, false, __VA_ARGS__)
#define FUCK_TreeNodeEx(...) \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::TreeNodeEx, FUCK::TreePop, false, __VA_ARGS__)
#define FUCK_TreeNodeV(...) \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::TreeNodeV, FUCK::TreePop, false, __VA_ARGS__)
#define FUCK_TreeNodeExV(...)                                       \
  FUCK_SUGAR_SCOPED_BOOL(ImGui::TreeNodeExV, FUCK::TreePop, false, \
                          __VA_ARGS__)

#define FUCK_TooltipOnHover(...)                                          \
  FUCK_SUGAR_SCOPED_BOOL_0(FUCK::Sugar::BeginTooltip, FUCK::EndTooltip, \
                            false)
#define FUCK_MainMenuBar(...)                                              \
  FUCK_SUGAR_SCOPED_BOOL_0(FUCK::BeginMainMenuBar, FUCK::EndMainMenuBar, \
                            false)
#define FUCK_MenuBar(...) \
  FUCK_SUGAR_SCOPED_BOOL_0(FUCK::BeginMenuBar, FUCK::EndMenuBar, false)

#define FUCK_Group \
  FUCK_SUGAR_SCOPED_VOID_0(FUCK::BeginGroup, FUCK::EndGroup)
#define FUCK_Tooltip \
  FUCK_SUGAR_SCOPED_VOID_0(ImGui::BeginTooltip, ImGui::EndTooltip)

#define FUCK_Font(...) \
  FUCK_SUGAR_SCOPED_VOID_N(FUCK::PushFont, FUCK::PopFont, __VA_ARGS__)
#define FUCK_AllowKeyboardFocus(...)                      \
  FUCK_SUGAR_SCOPED_VOID_N(ImGui::PushAllowKeyboardFocus, \
                            ImGui::PopAllowKeyboardFocus, __VA_ARGS__)
#define FUCK_ButtonRepeat(...)                                              \
  FUCK_SUGAR_SCOPED_VOID_N(ImGui::PushButtonRepeat, ImGui::PopButtonRepeat, \
                            __VA_ARGS__)
#define FUCK_ItemWidth(...)                                           \
  FUCK_SUGAR_SCOPED_VOID_N(ImGui::PushItemWidth, ImGui::PopItemWidth, \
                            __VA_ARGS__)
#define FUCK_TextWrapPos(...)                                             \
  FUCK_SUGAR_SCOPED_VOID_N(ImGui::PushTextWrapPos, ImGui::PopTextWrapPos, \
                            __VA_ARGS__)
#define FUCK_StyleColor(...)                      \
  FUCK_SUGAR_SCOPED_VOID_N(FUCK::PushStyleColor, \
                            FUCK::Sugar::PopStyleColor, __VA_ARGS__)
#define FUCK_StyleVar(...)                                                 \
  FUCK_SUGAR_SCOPED_VOID_N(ImGui::PushStyleVar, FUCK::Sugar::PopStyleVar, \
                            __VA_ARGS__)
#define FUCK_Indent(...) \
  FUCK_SUGAR_SCOPED_VOID_N(FUCK::Indent, FUCK::Sugar::Unindent, __VA_ARGS__)

#define FUCK_MenuItem(...) if (ImGui::MenuItem(__VA_ARGS__))

#define FUCK_Button(...) if (FUCK::Button(__VA_ARGS__))

#define FUCK_Selectable(...) if (FUCK::Selectable(__VA_ARGS__))

#define FUCK_Row FUCK::TableNextRow();

#define FUCK_Column FUCK::TableNextColumn();

#define FUCK_ID(...) \
  FUCK_SUGAR_SCOPED_VOID_N(FUCK::PushID, FUCK::PopID, __VA_ARGS__)

// Layout
#define FUCK_Horizontal(...)                                             \
  FUCK_SUGAR_SCOPED_VOID_N(ImGui::BeginHorizontal, ImGui::EndHorizontal, \
                            __VA_ARGS__)

#define FUCK_Vertical(...)                                           \
  FUCK_SUGAR_SCOPED_VOID_N(ImGui::BeginVertical, ImGui::EndVertical, \
                            __VA_ARGS__)

#define FUCK_SuspendLayout() \
  FUCK_SUGAR_SCOPED_VOID_0(ImGui::SuspendLayout, ImGui::ResumeLayout)