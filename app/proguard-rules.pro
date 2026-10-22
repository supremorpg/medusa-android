#-dontshrink
#-keepclasseswithmembernames class com.nvidia.devtech.*, com.wardrumstudios.utils.*

-keep public class com.nvidia.devtech.* { *; }

-keep public class com.wardrumstudios.utils.* { *; }

-keep public class com.supremo.launcher.main.* { *; }

-keep class com.startapp.** {
      *;
}

-keep class com.truenet.** {
      *;
}

-keepattributes Exceptions, InnerClasses, Signature, Deprecated, SourceFile,
LineNumberTable, *Annotation*, EnclosingMethod
-dontwarn com.startapp.**

-dontwarn org.jetbrains.annotations.**
