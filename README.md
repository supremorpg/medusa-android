# Medusa Android

Código-fonte do cliente Android **Medusa** (`com.supremo.launcher`) para desenvolvimento autorizado.

## Escopo da publicação

Este repositório é **fonte-only**. Para impedir a distribuição de material privado, não inclui:

- `google-services.json` e configurações Firebase;
- `local.properties`, chaves e configurações de assinatura;
- bibliotecas nativas prebuilt e assets de jogo, incluindo o conteúdo de `app/src/game/jniLibs`;
- AAR/JAR locais sem proveniência pública confirmada;
- APK/AAB, caches Gradle/NDK, logs e saídas de build.

Uma compilação funcional exige que a equipe autorizada disponibilize esses pré-requisitos fora do repositório. Não inclua esses materiais em commits, issues ou pull requests.

## Pré-requisitos

- JDK 17
- Android SDK com API 33
- Android NDK `26.2.11394342`
- Dependências privadas autorizadas nos caminhos esperados pelo projeto

Copie `local.properties.example` para `local.properties` e informe o SDK local. A configuração Firebase e as bibliotecas nativas devem ser providenciadas somente por canal privado autorizado.

## Build local

```sh
./gradlew --no-daemon assembleDebug
```

Quando todas as dependências privadas autorizadas estiverem presentes, o APK debug é produzido em `app/build/outputs/apk/`.

## CI controlada

`.github/workflows/android-build.yml` aceita somente disparos manuais do branch protegido. O workflow não baixa nem executa fontes enviadas por usuários. Ele requer os secrets de build definidos pela organização e entrega o resultado como artefato temporário do GitHub Actions.
