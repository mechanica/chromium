// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_

#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/native_theme_delegate.h"

namespace views {

// LabelButton is an alternative to TextButton, it's not focusable by default.
class VIEWS_EXPORT LabelButton : public CustomButton,
                                 public NativeThemeDelegate {
 public:
  LabelButton(ButtonListener* listener, const string16& text);
  virtual ~LabelButton();

  // Get or set the image shown for the specified button state.
  // GetImage returns the image for BS_NORMAL if the state's image is empty.
  const gfx::ImageSkia& GetImage(ButtonState for_state);
  void SetImage(ButtonState for_state, const gfx::ImageSkia& image);

  // Get or set the text shown on the button.
  const string16& GetText() const;
  void SetText(const string16& text);

  // Set the text color shown for the specified button state.
  void SetTextColor(ButtonState for_state, SkColor color);

  // Get or set the text's multi-line property to break on '\n', etc.
  bool GetTextMultiLine() const;
  void SetTextMultiLine(bool text_multi_line);

  // Get or set the horizontal alignment used for the button.
  // The optional image will lead the text, unless the button is right-aligned.
  Label::Alignment GetHorizontalAlignment() const;
  void SetHorizontalAlignment(Label::Alignment alignment);

  // Call set_min_size(gfx::Size()) to clear the monotonically increasing size.
  void set_min_size(const gfx::Size& min_size) { min_size_ = min_size; }
  void set_max_size(const gfx::Size& max_size) { max_size_ = max_size; }

  // Get or set the option to handle the return key; false by default.
  bool default_button() const { return default_button_; }
  void SetDefaultButton(bool default_button);

  // Get or set the option to use a native button appearance; false by default.
  bool native_theme() const { return native_theme_; }
  void SetNativeTheme(bool native_theme);

  // Overridden from CustomButton:
  virtual void StateChanged() OVERRIDE;

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;

 protected:
  // Overridden from NativeThemeDelegate:
  virtual ui::NativeTheme::Part GetThemePart() const OVERRIDE;
  virtual gfx::Rect GetThemePaintRect() const OVERRIDE;
  virtual ui::NativeTheme::State GetThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual const ui::Animation* GetThemeAnimation() const OVERRIDE;
  virtual ui::NativeTheme::State GetBackgroundThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual ui::NativeTheme::State GetForegroundThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;

  // Fill |params| with information about the button.
  virtual void GetExtraParams(ui::NativeTheme::ExtraParams* params) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Init);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, LabelAndImage);

  // The image and label shown in the button.
  ImageView* image_;
  Label* label_;

  // The images and colors for each button state.
  gfx::ImageSkia button_state_images_[BS_COUNT];
  SkColor button_state_colors_[BS_COUNT];

  // |min_size_| increases monotonically with the preferred size.
  gfx::Size min_size_;
  // |max_size_| may be set to clamp the preferred size.
  gfx::Size max_size_;

  // Flag indicating default handling of the return key via an accelerator.
  bool default_button_;

  // Flag indicating native theme styling (or Views styling) of the button.
  bool native_theme_;

  DISALLOW_COPY_AND_ASSIGN(LabelButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
