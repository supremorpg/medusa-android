package com.xyron.game.main.ui;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.util.TypedValue;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.content.res.ResourcesCompat;

import com.xyron.game.R;
import com.xyron.game.launcher.util.Util;

import com.xyron.game.main.SAMP;

import java.nio.charset.StandardCharsets;

public class RadialMenu {
    private static final int RADIAL_ICON_COLOR = 0xFFFFFFFF;
    private static final int RADIAL_TEXT_COLOR = 0xFFFFFFFF;
    private static final int RADIAL_BACKDROP_COLOR = 0x33000000;
    private static final int RADIAL_TEXT_SHADOW = 0xB0000000;
    private static final int[] RADIAL_BUTTON_IDS = {
            R.id.radial_button_00,
            R.id.radial_button_01,
            R.id.radial_button_02,
            R.id.radial_button_03,
            R.id.radial_button_04,
            R.id.radial_button_05,
            R.id.radial_button_06,
            R.id.radial_button_07,
            R.id.radial_button_08,
            R.id.radial_button_09
    };
    private static final int[] ACTIVE_BUTTON_IDS = {
            R.id.radial_button_00,
            R.id.radial_button_02,
            R.id.radial_button_05,
            R.id.radial_button_04
    };
    private ConstraintLayout radialLayout;
    private Activity activity;
    public static boolean menuVisible;
    private RadialVehicles mRadialVehicles;
    //int RadinhoSom;

    native void sendCommand(byte[] str);

    public RadialMenu(Activity activity) {
        this.activity = activity;

        // Inflar o layout radial
        ConstraintLayout layout = (ConstraintLayout) activity.getLayoutInflater().inflate(R.layout.radial, null);
        activity.addContentView(layout, new ConstraintLayout.LayoutParams(-1, -1));

        radialLayout = activity.findViewById(R.id.radial);
        radialLayout.setVisibility(View.GONE);
        applyTheme();

        //Som de radinho
        //RadinhoSom = SAMP.soundPool.load(SAMP.getInstance(), R.raw.somradinho, 0);

        // Configurar listeners para os botÃµes
        setListeners();
        menuVisible = false;

        // Esconder o layout inicialmente
        Util.HideLayout(radialLayout, false);
    }

    private void applyTheme() {
        Typeface labelTypeface = ResourcesCompat.getFont(activity, R.font.montserrat_regular);
        int[] iconIds = {
                R.id.radial_image_00,
                R.id.radial_image_01,
                R.id.radial_image_02,
                R.id.radial_image_03,
                R.id.radial_image_04,
                R.id.radial_image_05,
                R.id.radial_image_06,
                R.id.radial_image_07,
                R.id.radial_image_08,
                R.id.radial_image_09
        };
        int[] labelIds = {
                R.id.radial_text_00,
                R.id.radial_text_01,
                R.id.radial_text_02,
                R.id.radial_text_03,
                R.id.radial_text_04,
                R.id.radial_text_05,
                R.id.radial_text_06,
                R.id.radial_text_07,
                R.id.radial_text_08,
                R.id.radial_text_09
        };

        radialLayout.setBackgroundColor(RADIAL_BACKDROP_COLOR);

        ImageView closeIcon = activity.findViewById(R.id.imageView31);
        closeIcon.setImageTintList(ColorStateList.valueOf(RADIAL_TEXT_COLOR));

        for (int iconId : iconIds) {
            ImageView icon = activity.findViewById(iconId);
            icon.setImageTintList(ColorStateList.valueOf(RADIAL_ICON_COLOR));
        }

        for (int labelId : labelIds) {
            TextView label = activity.findViewById(labelId);
            label.setTextColor(RADIAL_TEXT_COLOR);
            label.setIncludeFontPadding(false);
            label.setSingleLine(true);
            label.setTextSize(TypedValue.COMPLEX_UNIT_SP, 8.4f);
            label.setShadowLayer(6f, 0f, 1f, RADIAL_TEXT_SHADOW);
            if (labelTypeface != null) {
                label.setTypeface(labelTypeface);
            }
        }

        applyFourOptionMode();
    }

    private void applyFourOptionMode() {
        for (int buttonId : RADIAL_BUTTON_IDS) {
            View button = activity.findViewById(buttonId);
            button.setVisibility(View.GONE);
            button.setClickable(false);
            button.setOnClickListener(null);
        }

        for (int buttonId : ACTIVE_BUTTON_IDS) {
            View button = activity.findViewById(buttonId);
            button.setVisibility(View.VISIBLE);
            button.setClickable(true);
            button.setAlpha(1.0f);
        }

        TextView helpLabel = activity.findViewById(R.id.radial_text_05);
        helpLabel.setText("Ajuda");

        ImageView helpIcon = activity.findViewById(R.id.radial_image_05);
        helpIcon.setImageResource(R.drawable.ic_hud_radial_ajuda);

        TextView settingsLabel = activity.findViewById(R.id.radial_text_04);
        settingsLabel.setText("Configuracao");
    }

    private void setListeners()
    {
        // BotÃ£o central para fechar o menu
        activity.findViewById(R.id.radial_close).setOnClickListener(view -> {
            hide();
        });

        // Configurar cliques nos botÃµes radiais
        setRadialButtonClick(R.id.radial_button_00, 0);
        setRadialButtonClick(R.id.radial_button_02, 2);
        setRadialButtonClick(R.id.radial_button_05, 5);
        setRadialButtonClick(R.id.radial_button_04, 4);
    }

    private void setRadialButtonClick(int buttonId, int actionId)
    {
        activity.findViewById(buttonId).setOnClickListener(view -> {
            if (actionId == 0)//Celular
            {
                hide();
                if (activity instanceof SAMP) {
                    ((SAMP) activity).showPhoneOverlay();
                    return;
                }
                sendCommand("/c".getBytes(StandardCharsets.UTF_8));
            }
            if (actionId == 1)//Gps
            {
                hide();
                sendCommand("/gps".getBytes(StandardCharsets.UTF_8));
            }
            if (actionId == 2)//Mochila
            {
                hide();
                if (activity instanceof SAMP) {
                    ((SAMP) activity).showInventoryOverlay();
                    return;
                }
                sendCommand("/m".getBytes(StandardCharsets.UTF_8));
            }
            if (actionId == 3)//Personagem
            {
                hide();
                sendCommand("/casas".getBytes(StandardCharsets.UTF_8));
            }
            if (actionId == 4)//Configuracoes
            {
                hide();
                if (activity instanceof SAMP) {
                    ((SAMP) activity).toggleHudSettingsFromRadial();
                    return;
                }
                sendCommand("/config".getBytes(StandardCharsets.UTF_8));
            }
            if (actionId == 5)//Missoes
            {
                hide();
                sendCommand("/ajuda".getBytes(StandardCharsets.UTF_8));
            }
            if (actionId == 6)//Loja Vip
            {
                hide();
                sendCommand("/loja".getBytes(StandardCharsets.UTF_8));
            }
            if (actionId == 7)//Atendimento
            {
                hide();
                sendCommand("/atm".getBytes(StandardCharsets.UTF_8));
            }
            if (actionId == 8)//Animacoes
            {
                hide();
                sendCommand("/anim".getBytes(StandardCharsets.UTF_8));
            }
            if (actionId == 9)//Registro RG
            {
                hide();
                sendCommand("/doc".getBytes(StandardCharsets.UTF_8));
            }
        });
    }

    public void show() {
        if(!menuVisible && !RadialVehicles.menuVisibleV)
        {
            Util.ShowLayout(radialLayout, true);
            menuVisible = true;
        }
    }

    public void hide() {
        if(menuVisible)
        {
            Util.HideLayout(radialLayout, true);
            menuVisible = false;
        }
    }
}
