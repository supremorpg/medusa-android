#!/usr/bin/env bash
set -Eeuo pipefail

# ============================================================================
# Supremo Launcher - Build + Auto ADB + Install + Launch
#
# Objetivos:
#   - Encontrar o adb automaticamente
#   - Iniciar o daemon adb
#   - Detectar BlueStacks já conectado
#   - Tentar conectar automaticamente ao BlueStacks
#   - Procurar portas ADB locais
#   - Nunca desinstalar o aplicativo
#   - Nunca executar pm clear
#   - Preservar todos os dados baixados pelo launcher
#   - Instalar o APK via adb install -r
#   - Detectar erros comuns de assinatura/versionamento
#   - Reiniciar somente o processo do app
#   - Iniciar a Activity principal
#
# Variáveis opcionais:
#
#   PACKAGE_NAME=com.supremo.launcher ./build-install.sh
#   ADB_SERIAL=127.0.0.1:5555 ./build-install.sh
#   BLUESTACKS_ADB_PORT=5555 ./build-install.sh
#   RESET_ADB=1 ./build-install.sh
#   SHOW_LOGS=1 ./build-install.sh
#   FRESH_BUILD=1 ./build-install.sh       # isola outputs/cache para diagnostico
#   GRADLE_MAX_WORKERS=4 ./build-install.sh
#   GRADLE_DAEMON=0 ./build-install.sh     # desativa daemon para diagnostico
#
# ============================================================================

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PACKAGE_NAME="${PACKAGE_NAME:-com.supremo.launcher}"
ADB_BIN="${ADB_BIN:-}"
ADB_SERIAL="${ADB_SERIAL:-}"
BLUESTACKS_ADB_PORT="${BLUESTACKS_ADB_PORT:-}"

GRADLE_TASK="${GRADLE_TASK:-:app:assembleDebug}"
APK_PATH="${APK_PATH:-}"
# Reuse the normal project outputs by default. A fresh temporary tree is much
# slower because it invalidates all Java, resource, and native incremental work.
FRESH_BUILD="${FRESH_BUILD:-0}"
ISOLATED_BUILD_ROOT="${ISOLATED_BUILD_ROOT:-}"
ISOLATED_PROJECT_CACHE="${ISOLATED_PROJECT_CACHE:-}"

# Keep the normal Gradle user home/daemon unless an isolated diagnostic build
# is needed. Dependency downloads stay cached even when project outputs must be
# separated from stale generated files.

RESET_ADB="${RESET_ADB:-0}"
AUTO_OPEN_BLUESTACKS="${AUTO_OPEN_BLUESTACKS:-1}"
WAIT_BOOT_SECONDS="${WAIT_BOOT_SECONDS:-30}"
SHOW_LOGS="${SHOW_LOGS:-0}"

# ----------------------------------------------------------------------------
# Cores
# ----------------------------------------------------------------------------

if [[ -t 1 ]]; then
    BOLD=$'\033[1m'
    GREEN=$'\033[0;32m'
    YELLOW=$'\033[0;33m'
    RED=$'\033[0;31m'
    CYAN=$'\033[0;36m'
    BLUE=$'\033[0;34m'
    RESET=$'\033[0m'
else
    BOLD=""
    GREEN=""
    YELLOW=""
    RED=""
    CYAN=""
    BLUE=""
    RESET=""
fi

info() {
    printf '%s[INFO]%s %s\n' "$CYAN" "$RESET" "$*"
}

ok() {
    printf '%s[OK]%s   %s\n' "$GREEN" "$RESET" "$*"
}

warn() {
    printf '%s[AVISO]%s %s\n' "$YELLOW" "$RESET" "$*" >&2
}

error() {
    printf '%s[ERRO]%s %s\n' "$RED" "$RESET" "$*" >&2
}

die() {
    error "$*"
    exit 1
}

section() {
    echo
    printf '%s%s============================================================%s\n' \
        "$BOLD" "$BLUE" "$RESET"
    printf '%s%s%s%s\n' "$BOLD" "$BLUE" "$*" "$RESET"
    printf '%s%s============================================================%s\n' \
        "$BOLD" "$BLUE" "$RESET"
}

on_error() {
    local exit_code=$?
    local line="${BASH_LINENO[0]:-desconhecida}"

    echo
    error "Falha inesperada na linha $line (exit code $exit_code)."
    exit "$exit_code"
}

trap on_error ERR

# ----------------------------------------------------------------------------
# Encontrar ADB
# ----------------------------------------------------------------------------

find_adb() {
    if [[ -n "$ADB_BIN" ]]; then
        [[ -x "$ADB_BIN" ]] || die "ADB_BIN existe, mas nao e executavel: $ADB_BIN"
        return
    fi

    if command -v adb >/dev/null 2>&1; then
        ADB_BIN="$(command -v adb)"
        return
    fi

    local candidates=(
        "${ANDROID_HOME:-}/platform-tools/adb"
        "${ANDROID_SDK_ROOT:-}/platform-tools/adb"
        "$HOME/Library/Android/sdk/platform-tools/adb"
        "/opt/homebrew/bin/adb"
        "/usr/local/bin/adb"
    )

    local candidate

    for candidate in "${candidates[@]}"; do
        [[ -n "$candidate" ]] || continue

        if [[ -x "$candidate" ]]; then
            ADB_BIN="$candidate"
            return
        fi
    done

    die "adb nao encontrado.

Instale Android Platform Tools ou configure:

    export ADB_BIN=/caminho/para/adb"
}

# ----------------------------------------------------------------------------
# Estado dos devices
# ----------------------------------------------------------------------------

device_state() {
    local serial="$1"

    "$ADB_BIN" devices 2>/dev/null |
        awk -v serial="$serial" '
            $1 == serial {
                print $2
                exit
            }
        '
}

is_online() {
    [[ "$(device_state "$1")" == "device" ]]
}

is_offline() {
    [[ "$(device_state "$1")" == "offline" ]]
}

is_unauthorized() {
    [[ "$(device_state "$1")" == "unauthorized" ]]
}

# ----------------------------------------------------------------------------
# Lista devices
# ----------------------------------------------------------------------------

print_devices() {
    "$ADB_BIN" devices -l || true
}

# ----------------------------------------------------------------------------
# Detectar device preferencial
# ----------------------------------------------------------------------------

find_existing_bluestacks_device() {
    "$ADB_BIN" devices 2>/dev/null |
        awk '
            NR > 1 && $2 == "device" &&
                ($1 ~ /^127\.0\.0\.1:/ || $1 ~ /^localhost:/ || $1 ~ /^emulator-/) {
                print $1
                exit
            }
        '
}

# ----------------------------------------------------------------------------
# Portas BlueStacks
# ----------------------------------------------------------------------------

add_port() {
    local port="$1"

    [[ "$port" =~ ^[0-9]+$ ]] || return 0

    case " ${CANDIDATE_PORTS[*]:-} " in
        *" $port "*)
            ;;
        *)
            CANDIDATE_PORTS+=("$port")
            ;;
    esac
}

discover_bluestacks_ports() {
    CANDIDATE_PORTS=()

    # Porta fornecida manualmente tem prioridade absoluta.
    if [[ -n "$BLUESTACKS_ADB_PORT" ]]; then
        add_port "$BLUESTACKS_ADB_PORT"
    fi

    # Porta padrao mais comum.
    add_port 5555

    # Algumas instancias/emuladores usam portas proximas.
    add_port 5556
    add_port 5557
    add_port 5558
    add_port 5559

    # Tentar extrair portas abertas por processos BlueStacks no macOS.
    if command -v lsof >/dev/null 2>&1; then
        while IFS= read -r port; do
            add_port "$port"
        done < <(
            lsof -nP -iTCP -sTCP:LISTEN 2>/dev/null |
                awk '
                    {
                        process=tolower($0)
                    }

                    process ~ /bluestack|hd-player/ {
                        address=$NF

                        sub(/^.*:/, "", address)

                        if (address ~ /^[0-9]+$/) {
                            print address
                        }
                    }
                ' | sort -n -u
        )
    fi
}

# ----------------------------------------------------------------------------
# Abrir BlueStacks no macOS
# ----------------------------------------------------------------------------

open_bluestacks_if_needed() {
    [[ "$AUTO_OPEN_BLUESTACKS" == "1" ]] || return 0
    [[ "$(uname -s)" == "Darwin" ]] || return 0

    if pgrep -if "BlueStacks" >/dev/null 2>&1; then
        ok "BlueStacks ja esta aberto."
        return 0
    fi

    info "Tentando abrir o BlueStacks..."

    if open -a "BlueStacks Air" >/dev/null 2>&1; then
        ok "BlueStacks Air iniciado."
        sleep 2
        return 0
    fi

    if open -a "BlueStacks" >/dev/null 2>&1; then
        ok "BlueStacks iniciado."
        sleep 2
        return 0
    fi

    warn "Nao consegui abrir o BlueStacks automaticamente."
}

# ----------------------------------------------------------------------------
# Ver se porta local esta aberta
# ----------------------------------------------------------------------------

port_is_open() {
    local port="$1"

    if command -v nc >/dev/null 2>&1; then
        nc -z -w 1 127.0.0.1 "$port" >/dev/null 2>&1
        return $?
    fi

    # Se nc nao existir, deixa o proprio adb tentar.
    return 0
}

# ----------------------------------------------------------------------------
# Tentar conectar em uma porta
# ----------------------------------------------------------------------------

try_adb_connect() {
    local port="$1"
    local serial="127.0.0.1:$port"

    if is_online "$serial"; then
        ADB_SERIAL="$serial"
        return 0
    fi

    if ! port_is_open "$port"; then
        return 1
    fi

    info "Tentando ADB em $serial..."

    "$ADB_BIN" connect "$serial" >/dev/null 2>&1 || true

    sleep 0.3

    if is_online "$serial"; then
        ADB_SERIAL="$serial"
        return 0
    fi

    return 1
}

# ----------------------------------------------------------------------------
# Conectar ao BlueStacks
# ----------------------------------------------------------------------------

connect_bluestacks() {
    section "[1/5] CONECTANDO AO BLUESTACKS"

    info "ADB: $ADB_BIN"

    if [[ "$RESET_ADB" == "1" ]]; then
        warn "RESET_ADB=1: reiniciando daemon ADB..."
        "$ADB_BIN" kill-server >/dev/null 2>&1 || true
    fi

    "$ADB_BIN" start-server >/dev/null

    ok "Daemon ADB iniciado."

    # ------------------------------------------------------------------------
    # Serial passado explicitamente
    # ------------------------------------------------------------------------

    if [[ -n "$ADB_SERIAL" ]]; then
        info "Device solicitado: $ADB_SERIAL"

        if [[ "$ADB_SERIAL" == *:* ]]; then
            "$ADB_BIN" connect "$ADB_SERIAL" >/dev/null 2>&1 || true
        fi

        if is_online "$ADB_SERIAL"; then
            ok "Device conectado: $ADB_SERIAL"
            return 0
        fi

        die "ADB_SERIAL=$ADB_SERIAL nao esta online.

Devices encontrados:

$("$ADB_BIN" devices -l)"
    fi

    # ------------------------------------------------------------------------
    # Ver se BlueStacks ja aparece no adb
    # ------------------------------------------------------------------------

    local existing

    existing="$(find_existing_bluestacks_device || true)"

    if [[ -n "$existing" ]]; then
        ADB_SERIAL="$existing"
        ok "BlueStacks detectado automaticamente: $ADB_SERIAL"
        return 0
    fi

    # ------------------------------------------------------------------------
    # Abrir BlueStacks
    # ------------------------------------------------------------------------

    open_bluestacks_if_needed

    discover_bluestacks_ports

    info "Procurando ADB do BlueStacks..."

    local port

    for port in "${CANDIDATE_PORTS[@]}"; do
        if try_adb_connect "$port"; then
            ok "BlueStacks conectado: $ADB_SERIAL"
            return 0
        fi
    done

    # ------------------------------------------------------------------------
    # Varredura adicional localhost
    # ------------------------------------------------------------------------

    if command -v nc >/dev/null 2>&1; then
        info "Fazendo busca adicional nas portas ADB locais..."

        for port in $(seq 5555 5585); do
            if try_adb_connect "$port"; then
                ok "BlueStacks encontrado automaticamente: $ADB_SERIAL"
                return 0
            fi
        done
    fi

    echo
    error "Nao encontrei nenhum BlueStacks acessivel via ADB."
    echo
    echo "Devices atuais:"
    print_devices
    echo
    echo "Tente manualmente:"
    echo
    echo "    adb start-server"
    echo "    adb connect 127.0.0.1:5555"
    echo "    adb devices -l"
    echo
    echo "Se o BlueStacks mostrar outra porta, rode:"
    echo
    echo "    BLUESTACKS_ADB_PORT=PORTA ./build-install.sh"
    echo

    exit 1
}

# ----------------------------------------------------------------------------
# Esperar Android inicializar
# ----------------------------------------------------------------------------

wait_for_android() {
    section "[2/5] VERIFICANDO ANDROID"

    info "Serial: $ADB_SERIAL"

    "$ADB_BIN" -s "$ADB_SERIAL" wait-for-device

    local elapsed=0
    local boot_completed=""

    while (( elapsed < WAIT_BOOT_SECONDS )); do
        boot_completed="$(
            "$ADB_BIN" -s "$ADB_SERIAL" shell getprop sys.boot_completed 2>/dev/null |
                tr -d '\r'
        )"

        if [[ "$boot_completed" == "1" ]]; then
            break
        fi

        sleep 1
        ((elapsed += 1)) || true
    done

    if [[ "$boot_completed" != "1" ]]; then
        warn "Android respondeu ao ADB, mas sys.boot_completed ainda nao retornou 1."
    else
        ok "Android inicializado."
    fi

    local model android_version sdk

    model="$(
        "$ADB_BIN" -s "$ADB_SERIAL" shell getprop ro.product.model 2>/dev/null |
            tr -d '\r'
    )"

    android_version="$(
        "$ADB_BIN" -s "$ADB_SERIAL" shell getprop ro.build.version.release 2>/dev/null |
            tr -d '\r'
    )"

    sdk="$(
        "$ADB_BIN" -s "$ADB_SERIAL" shell getprop ro.build.version.sdk 2>/dev/null |
            tr -d '\r'
    )"

    info "Modelo: ${model:-desconhecido}"
    info "Android: ${android_version:-?} / SDK ${sdk:-?}"
}

# ----------------------------------------------------------------------------
# Build
# ----------------------------------------------------------------------------

build_apk() {
    section "[3/5] GERANDO APK DEBUG"

    cd "$ROOT_DIR"

    [[ -x "./gradlew" ]] || die "gradlew nao encontrado ou sem permissao."

    info "Projeto: $ROOT_DIR"
    info "Gradle task: $GRADLE_TASK"

    # macOS/file-provider can leave duplicate intermediates such as
    # "reduce_size 3.xml" in packaged_res. A normal incremental build then
    # fails before compilation. Do not delete them; switch this invocation to
    # a fresh output tree that leaves the working tree and launcher data intact.
    if [[ "$FRESH_BUILD" != "1" && -d "$ROOT_DIR/app/build/intermediates/packaged_res" ]]; then
        local malformed_intermediate
        malformed_intermediate="$(find "$ROOT_DIR/app/build/intermediates/packaged_res" -type f -name '* *' -print -quit 2>/dev/null || true)"
        if [[ -n "$malformed_intermediate" ]]; then
            FRESH_BUILD=1
            warn "Intermediario com nome invalido detectado: $malformed_intermediate"
            warn "Usando outputs isolados nesta build; nenhum arquivo sera apagado."
        fi
    fi

    local gradle_args=(
        --console=plain
    )

    # The regular build keeps incremental outputs. A diagnostic isolated build
    # can be requested explicitly when stale intermediates are suspected.
    if [[ "$FRESH_BUILD" == "1" ]]; then
        if [[ -z "${GRADLE_USER_HOME:-}" ]]; then
            export GRADLE_USER_HOME="${TMPDIR:-/tmp}/supremo-launcher-gradle-home"
        fi
        if [[ -z "$ISOLATED_BUILD_ROOT" ]]; then
            ISOLATED_BUILD_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/supremo-launcher-build.XXXXXX")"
        fi
        if [[ -z "$ISOLATED_PROJECT_CACHE" ]]; then
            ISOLATED_PROJECT_CACHE="$(mktemp -d "${TMPDIR:-/tmp}/supremo-launcher-cache.XXXXXX")"
        fi
    fi

    if [[ "${GRADLE_DAEMON:-1}" == "0" ]]; then
        gradle_args+=(--no-daemon)
    else
        gradle_args+=(--daemon)
    fi

    # Keep the known Desktop/NFS workaround by default. Opt in only when the
    # filesystem is stable: GRADLE_WATCH_FS=1.
    if [[ "${GRADLE_WATCH_FS:-0}" == "0" ]]; then
        gradle_args+=(--no-watch-fs)
    fi

    if [[ -n "${GRADLE_MAX_WORKERS:-}" ]]; then
        gradle_args+=("--max-workers=$GRADLE_MAX_WORKERS")
    fi

    if [[ -n "$ISOLATED_PROJECT_CACHE" ]]; then
        mkdir -p "$ISOLATED_PROJECT_CACHE"
        gradle_args+=("--project-cache-dir=$ISOLATED_PROJECT_CACHE")
    fi

    if [[ "$FRESH_BUILD" != "1" ]]; then
        info "Build incremental: reutilizando app/build e caches do Gradle."
    else
        warn "Build diagnostica isolada: outputs e cache separados."
    fi

    if [[ "$FRESH_BUILD" != "1" && -z "$ISOLATED_BUILD_ROOT" && -z "$ISOLATED_PROJECT_CACHE" ]]; then
        info "Para corrigir intermediarios antigos, use FRESH_BUILD=1."
    fi

    if [[ -n "$ISOLATED_BUILD_ROOT" ]]; then
        mkdir -p "$ISOLATED_BUILD_ROOT"
        local init_script
        init_script="$(mktemp "${TMPDIR:-/tmp}/supremo-gradle-init.XXXXXX")"
        cat > "$init_script" <<EOF
allprojects { project ->
    project.buildDir = new File('$ISOLATED_BUILD_ROOT', project.path.replace(':', '_').replaceFirst('^_', 'root'))
}
EOF
        gradle_args+=("--init-script=$init_script")
    fi

    ./gradlew "${gradle_args[@]}" "$GRADLE_TASK"

    if [[ -z "$APK_PATH" ]]; then
        local default_apk="$ROOT_DIR/app/build/outputs/apk/debug/app-debug.apk"

        if [[ -n "$ISOLATED_BUILD_ROOT" ]]; then
            APK_PATH="$ISOLATED_BUILD_ROOT/rootapp/outputs/apk/debug/app-debug.apk"
        elif [[ -f "$default_apk" ]]; then
            APK_PATH="$default_apk"
        else
            APK_PATH="$(
                find "$ROOT_DIR/app/build/outputs/apk" \
                    -type f \
                    -name "*.apk" \
                    2>/dev/null |
                    head -n 1
            )"
        fi
    fi

    [[ -n "$APK_PATH" ]] || die "Nenhum APK foi encontrado."
    [[ -f "$APK_PATH" ]] || die "APK nao encontrado: $APK_PATH"

    ok "APK gerado:"
    echo "    $APK_PATH"

    if command -v stat >/dev/null 2>&1; then
        if [[ "$(uname -s)" == "Darwin" ]]; then
            local size
            size="$(stat -f%z "$APK_PATH" 2>/dev/null || true)"

            if [[ -n "$size" ]]; then
                info "Tamanho: $((size / 1024 / 1024)) MB"
            fi
        fi
    fi
}

# ----------------------------------------------------------------------------
# Instalar APK preservando dados
# ----------------------------------------------------------------------------

install_apk() {
    section "[4/5] INSTALANDO APK"

    info "Package: $PACKAGE_NAME"
    info "Device:  $ADB_SERIAL"

    local existed_before=0

    if "$ADB_BIN" -s "$ADB_SERIAL" shell pm path "$PACKAGE_NAME" \
        >/dev/null 2>&1; then

        existed_before=1
        ok "Aplicativo ja instalado."

        info "Usando atualizacao in-place para preservar os dados."
    else
        info "Aplicativo ainda nao esta instalado."
    fi

    local output
    local exit_code=0

    set +e

    output="$(
        "$ADB_BIN" -s "$ADB_SERIAL" install \
            -r \
            -d \
            "$APK_PATH" 2>&1
    )"

    exit_code=$?

    set -e

    echo "$output"

    if [[ $exit_code -ne 0 ]] || ! grep -q "Success" <<<"$output"; then

        echo

        if grep -q "INSTALL_FAILED_UPDATE_INCOMPATIBLE" <<<"$output"; then
            error "A assinatura do APK novo e diferente da versao instalada."
            echo
            echo "Para preservar os dados, o APK precisa ser assinado com a MESMA chave."
            echo
            echo "Nao vou desinstalar automaticamente porque isso poderia apagar"
            echo "os dados baixados pelo launcher."
            exit 1
        fi

        if grep -q "INSTALL_FAILED_VERSION_DOWNGRADE" <<<"$output"; then
            error "Android bloqueou downgrade de versao."
            echo
            echo "Verifique versionCode/versionName do APK."
            exit 1
        fi

        if grep -q "INSTALL_FAILED_INSUFFICIENT_STORAGE" <<<"$output"; then
            die "BlueStacks esta sem espaco suficiente."
        fi

        if grep -q "device offline" <<<"$output"; then
            die "O dispositivo ficou offline durante a instalacao."
        fi

        if grep -q "unauthorized" <<<"$output"; then
            die "ADB nao autorizado."
        fi

        die "Falha ao instalar APK."
    fi

    ok "APK instalado com sucesso."

    if [[ "$existed_before" == "1" ]]; then
        ok "Atualizacao feita sem uninstall/pm clear."
        ok "Dados existentes de $PACKAGE_NAME preservados."
    fi

    if ! "$ADB_BIN" -s "$ADB_SERIAL" shell pm path "$PACKAGE_NAME" \
        >/dev/null 2>&1; then
        die "Instalacao retornou sucesso, mas o package nao foi encontrado."
    fi
}

# ----------------------------------------------------------------------------
# Iniciar app
# ----------------------------------------------------------------------------

launch_app() {
    section "[5/5] INICIANDO SUPREMO"

    info "Encerrando apenas o processo atual..."

    "$ADB_BIN" -s "$ADB_SERIAL" shell \
        am force-stop "$PACKAGE_NAME" >/dev/null 2>&1 || true

    local activity

    activity="$(
        "$ADB_BIN" -s "$ADB_SERIAL" shell \
            cmd package resolve-activity \
            --brief \
            -c android.intent.category.LAUNCHER \
            "$PACKAGE_NAME" \
            2>/dev/null |
            tr -d '\r' |
            tail -n 1
    )"

    if [[ -n "$activity" ]] && [[ "$activity" == *"/"* ]]; then
        info "Activity detectada: $activity"

        "$ADB_BIN" -s "$ADB_SERIAL" shell \
            am start \
            -W \
            -n "$activity"
    else
        warn "Nao consegui resolver a Activity principal."
        info "Usando launcher via monkey..."

        "$ADB_BIN" -s "$ADB_SERIAL" shell \
            monkey \
            -p "$PACKAGE_NAME" \
            -c android.intent.category.LAUNCHER \
            1 >/dev/null
    fi

    sleep 1

    local pid

    pid="$(
        "$ADB_BIN" -s "$ADB_SERIAL" shell pidof "$PACKAGE_NAME" \
            2>/dev/null |
            tr -d '\r'
    )"

    if [[ -n "$pid" ]]; then
        ok "Aplicativo rodando. PID: $pid"
    else
        warn "O app foi iniciado, mas nao encontrei o PID."
        warn "Se ele fechou sozinho, provavelmente ocorreu um crash."
    fi

    if [[ "$SHOW_LOGS" == "1" ]]; then
        echo
        info "Ultimas mensagens relevantes do logcat:"
        echo

        "$ADB_BIN" -s "$ADB_SERIAL" logcat -d -v time |
            grep -iE \
                "$PACKAGE_NAME|AndroidRuntime|FATAL EXCEPTION|fatal signal" |
            tail -n 100 || true
    fi
}

# ----------------------------------------------------------------------------
# Resumo
# ----------------------------------------------------------------------------

finish() {
    echo
    printf '%s%s============================================================%s\n' \
        "$BOLD" "$GREEN" "$RESET"

    printf '%s%s  SUPREMO INSTALADO COM SUCESSO%s\n' \
        "$BOLD" "$GREEN" "$RESET"

    printf '%s%s============================================================%s\n' \
        "$BOLD" "$GREEN" "$RESET"

    echo
    echo "Device:  $ADB_SERIAL"
    echo "Package: $PACKAGE_NAME"
    echo "APK:     $APK_PATH"
    echo
    echo "Os dados existentes do aplicativo nao foram apagados."
    echo
}

# ============================================================================
# MAIN
# ============================================================================

find_adb
connect_bluestacks
wait_for_android
build_apk
install_apk
launch_app
finish