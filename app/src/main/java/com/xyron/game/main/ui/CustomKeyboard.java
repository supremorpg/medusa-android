package com.xyron.game.main.ui;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.text.Editable;
import android.text.InputFilter;
import android.util.DisplayMetrics;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;

import com.xyron.game.R;

import java.util.ArrayList;

public class CustomKeyboard {
    private static final int BASE_BOTTOM_MARGIN_DP = 18;
    private static final int MAX_CHAT_INPUT_CHARS = 144;

    public interface InputListener {
        void OnInputEnd(String str);
    }

    private LinearLayout mInputLayout = null;
    private EditText mInputEt = null;
    private View mRootLayout = null;

    private Button mButtonSlash = null;
    private Button mButtonHistoryNext = null;
    private Button mButtonHistoryPrev = null;
    private Button mButtonSend = null;
    private Button mButtonClose = null;

    private final Activity mContext;
    private boolean mIsShowing = false;
    private String mSavedInput = null;
    private final int mMaxHistory = 20;
    private final ArrayList<String> mInputHistory;
    private int mCurrentHistoryMessage = 0;
    private int mLastKeyboardHeight = 0;
    private final int mBaseBottomMarginPx;
    private final int mFallbackKeyboardHeightPx;

    public boolean IsShowing() {
        return mIsShowing;
    }

    public CustomKeyboard(Activity act) {
        mContext = act;
        mBaseBottomMarginPx = Math.round(
                act.getResources().getDisplayMetrics().density * BASE_BOTTOM_MARGIN_DP
        );
        DisplayMetrics metrics = act.getResources().getDisplayMetrics();
        mFallbackKeyboardHeightPx = Math.round(Math.max(
                metrics.heightPixels * 0.38f,
                act.getResources().getDisplayMetrics().density * 180.0f
        ));

        ConstraintLayout layout = (ConstraintLayout) act.getLayoutInflater().inflate(R.layout.layout_keyboard, null);
        mRootLayout = layout;
        mContext.addContentView(layout, new ConstraintLayout.LayoutParams(-1, -1));

        mInputLayout = layout.findViewById(R.id.main_input);
        mInputEt = layout.findViewById(R.id.main_et_input);
        mButtonSlash = layout.findViewById(R.id.main_but_input_slash);
        mButtonHistoryPrev = layout.findViewById(R.id.main_but_input_prev);
        mButtonHistoryNext = layout.findViewById(R.id.main_but_input_next);
        mButtonSend = layout.findViewById(R.id.main_but_input_send);
        mButtonClose = layout.findViewById(R.id.main_but_input_close);
        mInputHistory = new ArrayList<>();
        installInsetsListener();

        mButtonSlash.setOnClickListener(view ->
                mInputEt.getText().insert(mInputEt.getSelectionStart(), "/")
        );

        mButtonHistoryPrev.setOnClickListener(view -> {
            mCurrentHistoryMessage--;
            if (mCurrentHistoryMessage < 0) {
                mCurrentHistoryMessage = 0;
            }
            if (mCurrentHistoryMessage <= 0) {
                mInputEt.setText("");
                return;
            }

            mInputEt.setText(mInputHistory.get(mCurrentHistoryMessage - 1));
            mInputEt.setSelection(mInputEt.getText().length());
        });

        mButtonHistoryNext.setOnClickListener(view -> {
            mCurrentHistoryMessage++;
            if ((mCurrentHistoryMessage - 1) >= mInputHistory.size()) {
                mCurrentHistoryMessage--;
            }
            if (mCurrentHistoryMessage <= 0) {
                return;
            }

            mInputEt.setText(mInputHistory.get(mCurrentHistoryMessage - 1));
            mInputEt.setSelection(mInputEt.getText().length());
        });

        mButtonSend.setOnClickListener(view -> submitInput());
        mButtonClose.setOnClickListener(view -> HideInputLayout());
        mInputEt.setFilters(new InputFilter[]{new InputFilter.LengthFilter(MAX_CHAT_INPUT_CHARS)});

        mInputEt.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView textView, int actionId, KeyEvent keyEvent) {
                if (actionId == EditorInfo.IME_ACTION_DONE
                        || actionId == EditorInfo.IME_ACTION_NEXT
                        || actionId == EditorInfo.IME_ACTION_SEND
                        || (keyEvent != null
                        && keyEvent.getAction() == KeyEvent.ACTION_DOWN
                        && keyEvent.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                    submitInput();
                    return true;
                }
                return false;
            }
        });

        HideInputLayout();
    }

    public void onHeightChanged(int height) {
        mLastKeyboardHeight = Math.max(height, 0);
        int visibleHeight = mLastKeyboardHeight > 0 ? mLastKeyboardHeight : mFallbackKeyboardHeightPx;
        applyBottomOffset(mIsShowing ? visibleHeight : 0);
    }

    private void installInsetsListener() {
        if (mRootLayout == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return;
        }
        mRootLayout.setOnApplyWindowInsetsListener((view, insets) -> {
            int imeHeight = insets.getInsets(WindowInsets.Type.ime()).bottom;
            if (imeHeight > 0 || mIsShowing) {
                onHeightChanged(imeHeight);
            }
            return insets;
        });
    }

    public void ShowInputLayout() {
        if (mIsShowing) {
            return;
        }

        mIsShowing = true;
        mCurrentHistoryMessage = 0;

        if (mSavedInput != null) {
            mInputEt.setText(mSavedInput);
            mInputEt.setSelection(mInputEt.getText().length());
        }

        applyBottomOffset(resolveKeyboardHeight(true));
        mInputLayout.setVisibility(View.VISIBLE);

        mInputEt.postDelayed(() -> {
            mInputEt.requestFocus();
            mInputEt.setSelection(mInputEt.getText() != null ? mInputEt.getText().length() : 0);
            InputMethodManager imm = (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.showSoftInput(mInputEt, InputMethodManager.SHOW_IMPLICIT);
            }
        }, 60);
        scheduleKeyboardOffsetRefresh();
    }

    public void ClearSavedInput() {
        mSavedInput = "";
        if (mInputEt != null) {
            mInputEt.setText("");
        }
    }

    public void HideInputLayout() {
        mIsShowing = false;
        mCurrentHistoryMessage = 0;

        if (mInputEt != null && mInputEt.getEditableText() != null) {
            mSavedInput = mInputEt.getEditableText().toString();
        }

        if (mInputEt != null) {
            InputMethodManager imm = (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.hideSoftInputFromWindow(mInputEt.getWindowToken(), 0);
            }
            mInputEt.clearFocus();
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && mContext.getWindow() != null) {
            WindowInsetsController controller = mContext.getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.ime());
            }
        }

        View currentFocus = mContext.getCurrentFocus();
        if (currentFocus != null) {
            InputMethodManager imm = (InputMethodManager) mContext.getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.hideSoftInputFromWindow(currentFocus.getWindowToken(), 0);
            }
        }

        applyBottomOffset(0);
        if (mInputLayout != null) {
            mInputLayout.setVisibility(View.GONE);
        }
    }

    private void applyBottomOffset(int keyboardHeight) {
        if (mInputLayout == null) {
            return;
        }

        ViewGroup.LayoutParams rawParams = mInputLayout.getLayoutParams();
        if (!(rawParams instanceof ViewGroup.MarginLayoutParams)) {
            return;
        }

        ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) rawParams;
        int targetBottom = mBaseBottomMarginPx;
        if (keyboardHeight > 0) {
            targetBottom += keyboardHeight;
        }

        if (params.bottomMargin != targetBottom) {
            params.bottomMargin = targetBottom;
            mInputLayout.setLayoutParams(params);
        }
    }

    private void scheduleKeyboardOffsetRefresh() {
        if (mInputLayout == null) {
            return;
        }
        int[] delays = {120, 280, 520, 820};
        for (int delay : delays) {
            mInputLayout.postDelayed(() -> {
                if (mIsShowing) {
                    applyBottomOffset(resolveKeyboardHeight(true));
                }
            }, delay);
        }
    }

    private int resolveKeyboardHeight(boolean useFallback) {
        int insetsHeight = getImeInsetsHeight();
        if (insetsHeight > 0) {
            mLastKeyboardHeight = insetsHeight;
            return insetsHeight;
        }
        if (mLastKeyboardHeight > 0) {
            return mLastKeyboardHeight;
        }
        return useFallback ? mFallbackKeyboardHeightPx : 0;
    }

    private int getImeInsetsHeight() {
        if (mRootLayout == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return 0;
        }
        WindowInsets insets = mRootLayout.getRootWindowInsets();
        if (insets == null) {
            return 0;
        }
        return insets.getInsets(WindowInsets.Type.ime()).bottom;
    }

    private void submitInput() {
        Editable editableText = mInputEt.getText();
        if (editableText == null) {
            return;
        }

        String str = editableText.toString();
        mInputEt.setText("");
        OnInputEnd(str);
    }

    private void OnInputEnd(String str) {
        if (mInputHistory.size() >= mMaxHistory) {
            mInputHistory.remove(mInputHistory.size() - 1);
        }
        mInputHistory.add(0, str);

        InputListener listener = (InputListener) mContext;
        listener.OnInputEnd(str);
    }
}
