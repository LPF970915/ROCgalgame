#include "app_language.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>

namespace {
struct LanguageDef {
  const char *config_value;
  const char *display_label;
};

constexpr std::array<LanguageDef, 12> kLanguages = {{
    {"zh", u8"\u7b80\u4f53\u4e2d\u6587"},
    {"zh-Hant", u8"\u7e41\u9ad4\u4e2d\u6587"},
    {"en", "English"},
    {"es", u8"Espa\u00f1ol"},
    {"fr", u8"Fran\u00e7ais"},
    {"de", "Deutsch"},
    {"ja", u8"\u65e5\u672c\u8a9e"},
    {"ko", u8"\ud55c\uad6d\uc5b4"},
    {"ar", u8"\u0627\u0644\u0639\u0631\u0628\u064a\u0629"},
    {"ru", u8"\u0420\u0443\u0441\u0441\u043a\u0438\u0439"},
    {"pt", u8"Portugu\u00eas"},
    {"vi", u8"Ti\u1ebfng Vi\u1ec7t"},
}};

constexpr size_t kTextCount = static_cast<size_t>(AppTextId::ContributionValue) + 1;

constexpr std::array<const char *, kTextCount> kTextZh = {{
    u8"ROC\u9605\u8bfb\u5668",
    u8"\u7cfb\u7edf\u8bbe\u7f6e",
    u8"\u6309\u952e\u8bf4\u660e",
    u8"\u6309\u952e\u6821\u51c6",
    u8"\u6e05\u9664\u5386\u53f2",
    u8"\u6e05\u9664\u7f13\u5b58",
    u8"\u6587\u672c\u8bbe\u7f6e",
    u8"\u8d21\u732e\u8005\u5934\u50cf",
    u8"\u8054\u7cfb\u6211",
    u8"\u7248\u672c\u66f4\u65b0",
    u8"URL\u5165\u53e3",
    u8"\u9000\u51fa",
    u8"\u6309A\u5b8c\u5168\u9000\u51fa\u7a0b\u5e8f",
    u8"\u6309\u952e\u97f3\u91cf",
    u8"\u5c4f\u5e55\u4eae\u5ea6",
    u8"\u81ea\u52a8\u4f11\u7720",
    u8"\u4f11\u7720\u95f4\u9694",
    u8"\u7cfb\u7edf\u8bed\u8a00",
    u8"\u5f00\u542f",
    u8"\u5173\u95ed",
    u8"\u6e05\u9664",
    u8"\u6e05\u9664\u7f13\u5b58",
    u8"\u6e05\u9664\u5386\u53f2",
    u8"\u80cc\u666f\u989c\u8272",
    u8"\u5b57\u4f53\u989c\u8272",
    u8"\u5b57\u53f7\u5927\u5c0f",
    u8"TXT\u8f6c\u7801",
    u8"\u5f00\u59cb\u8f6c\u7801",
    u8"\u5f53\u524d\u7248\u672c",
    u8"\u68c0\u6d4b\u5e76\u66f4\u65b0",
    u8"\u6309 A \u5f00\u59cb\u68c0\u67e5\u66f4\u65b0",
    u8"\u6ca1\u6709\u68c0\u6d4b\u5230\u7f51\u7edc\u8fde\u63a5",
    u8"\u68c0\u6d4b\u5230\u66f4\u65b0\uff0c\u6b63\u5728\u4e0b\u8f7d",
    u8"\u5df2\u4e0b\u8f7d\u5b89\u88c5\u5305",
    u8"\u91cd\u542f\u5b89\u88c5",
    u8"\u5df2\u662f\u6700\u65b0\u7248\u672c",
    u8"\u4e0b\u8f7d\u5931\u8d25\uff0c\u8bf7\u7a0d\u540e\u91cd\u8bd5",
    u8"34XX SP \u4e13\u7528\u6620\u5c04",
    u8"35XX H \u4e13\u7528\u6620\u5c04",
    u8"H700 \u901a\u7528\u6620\u5c04",
    u8"Trimui Brick \u4e13\u7528\u6620\u5c04",
    u8"\u6d4f\u89c8\u4e66\u67b6\uff0c\u9605\u8bfb\u65f6\u7ffb\u9875\u6216\u6eda\u52a8",
    u8"\u786e\u8ba4\uff0c\u6253\u5f00\u4e66\u7c4d\uff1b\u9605\u8bfb\u5668\u4e2d\u91cd\u7f6e\u89c6\u56fe",
    u8"\u8fd4\u56de\u4e0a\u4e00\u7ea7\uff1b\u9605\u8bfb\u65f6\u9000\u51fa",
    u8"\u4e66\u67b6\u6807\u8bb0\u6536\u85cf\uff1b\u9605\u8bfb\u65f6\u663e\u793a\u8fdb\u5ea6",
    u8"\u4e66\u67b6\u53d6\u6d88\u6536\u85cf\uff1b\u9605\u8bfb\u65f6\u6253\u5f00\u7ae0\u8282\u76ee\u5f55",
    u8"\u4e66\u67b6\u5207\u6362\u5206\u7c7b\uff1bPDF/EPUB \u7f29\u653e",
    u8"PDF/EPUB \u5de6\u65cb / \u53f3\u65cb",
    u8"\u6253\u5f00\u6216\u5173\u95ed\u8bbe\u7f6e\u83dc\u5355",
    u8"Start \u4e0e Select \u540c\u65f6\u6309\u4e0b\u53ef\u5b8c\u5168\u9000\u51fa\u7a0b\u5e8f",
    u8"\u8c03\u9ad8 / \u8c03\u4f4e\u97f3\u91cf",
    u8"\u6253\u8d4f\u5e76\u8054\u7cfb\u6211\u6dfb\u52a0\u60a8\u7684\u4e13\u5c5e\u5934\u50cf",
    u8"\u8d21\u732e\u503c",
}};

constexpr std::array<const char *, kTextCount> kTextZhHant = {{
    u8"ROC\u95b1\u8b80\u5668",
    u8"\u7cfb\u7d71\u8a2d\u5b9a",
    u8"\u6309\u9375\u8aaa\u660e",
    u8"\u6309\u9375\u6821\u6e96",
    u8"\u6e05\u9664\u6b77\u53f2",
    u8"\u6e05\u9664\u5feb\u53d6",
    u8"\u6587\u672c\u8a2d\u5b9a",
    u8"\u8ca2\u737b\u8005\u982d\u50cf",
    u8"\u806f\u7d61\u6211",
    u8"\u7248\u672c\u66f4\u65b0",
    u8"URL\u5165\u53e3",
    u8"\u9000\u51fa",
    u8"\u6309A\u5b8c\u5168\u9000\u51fa\u7a0b\u5f0f",
    u8"\u6309\u9375\u97f3\u91cf",
    u8"\u87a2\u5e55\u4eae\u5ea6",
    u8"\u81ea\u52d5\u4f11\u7720",
    u8"\u4f11\u7720\u9593\u9694",
    u8"\u7cfb\u7d71\u8a9e\u8a00",
    u8"\u958b\u555f",
    u8"\u95dc\u9589",
    u8"\u6e05\u9664",
    u8"\u6e05\u9664\u5feb\u53d6",
    u8"\u6e05\u9664\u6b77\u53f2",
    u8"\u80cc\u666f\u984f\u8272",
    u8"\u5b57\u9ad4\u984f\u8272",
    u8"\u5b57\u865f\u5927\u5c0f",
    u8"TXT\u8f49\u78bc",
    u8"\u958b\u59cb\u8f49\u78bc",
    u8"\u76ee\u524d\u7248\u672c",
    u8"\u6aa2\u6e2c\u4e26\u66f4\u65b0",
    u8"\u6309 A \u958b\u59cb\u6aa2\u67e5\u66f4\u65b0",
    u8"\u672a\u5075\u6e2c\u5230\u7db2\u8def\u9023\u7dda",
    u8"\u5075\u6e2c\u5230\u66f4\u65b0\uff0c\u6b63\u5728\u4e0b\u8f09",
    u8"\u5df2\u4e0b\u8f09\u5b89\u88dd\u5957\u4ef6",
    u8"\u91cd\u555f\u5b89\u88dd",
    u8"\u5df2\u662f\u6700\u65b0\u7248\u672c",
    u8"\u4e0b\u8f09\u5931\u6557\uff0c\u8acb\u7a0d\u5f8c\u91cd\u8a66",
    u8"34XX SP \u5c08\u7528\u6620\u5c04",
    u8"35XX H \u5c08\u7528\u6620\u5c04",
    u8"H700 \u901a\u7528\u6620\u5c04",
    u8"Trimui Brick \u5c08\u7528\u6620\u5c04",
    u8"\u700f\u89bd\u66f8\u67b6\uff0c\u95b1\u8b80\u6642\u7ffb\u9801\u6216\u6372\u52d5",
    u8"\u78ba\u8a8d\uff0c\u958b\u555f\u66f8\u7c4d\uff1b\u95b1\u8b80\u5668\u4e2d\u91cd\u8a2d\u8996\u5716",
    u8"\u8fd4\u56de\u4e0a\u4e00\u5c64\uff1b\u95b1\u8b80\u6642\u96e2\u958b",
    u8"\u66f8\u67b6\u6a19\u8a18\u6536\u85cf\uff1b\u95b1\u8b80\u6642\u986f\u793a\u9032\u5ea6",
    u8"\u66f8\u67b6\u53d6\u6d88\u6536\u85cf\uff1b\u95b1\u8b80\u6642\u958b\u555f\u7ae0\u7bc0\u76ee\u9304",
    u8"\u66f8\u67b6\u5207\u63db\u5206\u985e\uff1bPDF/EPUB \u7e2e\u653e",
    u8"PDF/EPUB \u5de6\u65cb / \u53f3\u65cb",
    u8"\u958b\u555f\u6216\u95dc\u9589\u8a2d\u5b9a\u9078\u55ae",
    u8"Start \u8207 Select \u540c\u6642\u6309\u4e0b\u53ef\u5b8c\u5168\u96e2\u958b\u7a0b\u5f0f",
    u8"\u8abf\u9ad8 / \u8abf\u4f4e\u97f3\u91cf",
    u8"\u6253\u8cde\u4e26\u806f\u7d61\u6211\u65b0\u589e\u60a8\u7684\u5c08\u5c6c\u982d\u50cf",
    u8"\u8ca2\u737b\u503c",
}};

constexpr std::array<const char *, kTextCount> kTextEn = {{
    "ROCreader",
    "System",
    "Key Guide",
    "Key Calib",
    "Clear History",
    "Clear Cache",
    "Text Settings",
    "Contributors",
    "Contact Me",
    "Updates",
    "URL Entry",
    "Exit",
    "Press A to fully exit",
    "Key Sound",
    "Brightness",
    "Auto Sleep",
    "Sleep Timer",
    "System Language",
    "On",
    "Off",
    "Clear",
    "Clear Cache",
    "Clear History",
    "Background",
    "Font Color",
    "Font Size",
    "TXT Transcode",
    "Start",
    "Current Version",
    "Check for Updates",
    "Press A to check",
    "No network connection",
    "Update found, downloading",
    "Package downloaded",
    "Restart to install",
    "Already up to date",
    "Download failed, try again later",
    "34XX SP Mapping",
    "35XX H Mapping",
    "H700 Standard Mapping",
    "Trimui Brick Mapping",
    "Browse the shelf; turn pages or scroll while reading",
    "Confirm and open books; reset view in the reader",
    "Go back; exit the reader",
    "Add favorite on shelf; show reading progress",
    "Remove favorite on shelf; open chapter list while reading",
    "Switch shelf categories; zoom PDF/EPUB",
    "Rotate PDF/EPUB left or right",
    "Open or close the settings menu",
    "Hold Start and Select together to fully exit",
    "Raise or lower volume",
    "Reward and contact me to add your exclusive avatar",
    "Contribution",
}};

constexpr std::array<const char *, kTextCount> kTextEs = {{
    "ROCreader", "Sistema", "Guia teclas", "Calibrar", "Borrar historial", "Borrar cache", "Texto", "Colaboradores",
    "Contacto", "Actualizaciones", "Entrada URL", "Salir", "Pulsa A para salir", "Sonido teclas", "Brillo",
    "Susp. automatica", "Temporizador", "Idioma sistema", "Activado", "Desactivado", "Borrar",
    "Borrar cache", "Borrar historial", "Fondo", "Color fuente", "Tamano fuente", "TXT",
    "Iniciar", "Version actual", "Buscar actualizacion", "Pulsa A para buscar", "Sin conexion de red",
    "Actualizacion encontrada, descargando", "Paquete descargado", "Reinicia para instalar",
    "Ya esta actualizado", "Descarga fallida, intenta mas tarde", "Mapa 34XX SP", "Mapa 35XX H",
    "Mapa general H700", "Mapa Trimui Brick", "Navegar por la estanteria; pasar pagina o desplazar en lectura",
    "Confirmar y abrir libros; restablecer vista en el lector", "Volver; salir del lector",
    "Anadir favorito en estanteria; mostrar progreso de lectura", "Quitar favorito de la estanteria; abrir indice de capitulos durante la lectura",
    "Cambiar categorias; zoom en PDF/EPUB", "Girar PDF/EPUB a izquierda o derecha",
    "Abrir o cerrar el menu de ajustes", "Mantener Start y Select para salir por completo",
    "Subir o bajar el volumen", "Apoyame y contactame para anadir tu avatar exclusivo", "Contribucion",
}};

constexpr std::array<const char *, kTextCount> kTextFr = {{
    "ROCreader", "Systeme", "Guide touches", "Calibrer", "Effacer hist.", "Vider cache", "Texte", "Contributeurs",
    "Contact", "Mises a jour", "Entree URL", "Quitter", "Appuyez sur A pour quitter", "Son touches", "Luminosite",
    "Veille auto", "Minuteur veille", "Langue systeme", "Actif", "Arret", "Effacer", "Vider cache",
    "Effacer hist.", "Fond", "Couleur texte", "Taille police", "TXT", "Demarrer", "Version actuelle",
    "Verifier les mises a jour", "Appuyez sur A pour verifier", "Aucun reseau detecte",
    "Mise a jour detectee, telechargement", "Paquet telecharge", "Redemarrez pour installer",
    "Deja a jour", "Echec du telechargement, reessayez", "Profil 34XX SP", "Profil 35XX H",
    "Profil H700 standard", "Profil Trimui Brick", "Parcourir la bibliotheque; tourner ou faire defiler en lecture",
    "Confirmer et ouvrir les livres; reinitialiser la vue du lecteur", "Retour; quitter le lecteur",
    "Ajouter aux favoris; afficher la progression de lecture", "Retirer des favoris; ouvrir la liste des chapitres en lecture",
    "Changer de categorie; zoom PDF/EPUB", "Rotation PDF/EPUB gauche ou droite",
    "Ouvrir ou fermer le menu des reglages", "Maintenir Start et Select pour quitter completement",
    "Augmenter ou baisser le volume", "Soutenez-moi et contactez-moi pour ajouter votre avatar exclusif", "Contribution",
}};

constexpr std::array<const char *, kTextCount> kTextDe = {{
    "ROCreader", "System", "Tastenhilfe", "Tasten kal.", "Verlauf loschen", "Cache leeren", "Text", "Mitwirkende",
    "Kontakt", "Updates", "URL Eingang", "Beenden", "A drucken zum Beenden", "Tastenton", "Helligkeit",
    "Auto-Ruhe", "Ruhe-Timer", "Systemsprache", "Ein", "Aus", "Loschen", "Cache leeren",
    "Verlauf loschen", "Hintergrund", "Schriftfarbe", "Schriftgrosse", "TXT", "Start",
    "Aktuelle Version", "Auf Updates prufen", "A zum Prufen drucken", "Keine Netzwerkverbindung",
    "Update gefunden, wird geladen", "Paket heruntergeladen", "Zum Installieren neu starten",
    "Bereits aktuell", "Download fehlgeschlagen, spater erneut", "34XX SP Belegung", "35XX H Belegung",
    "H700 Standardbelegung", "Trimui Brick Belegung", "Im Regal navigieren; beim Lesen blattern oder scrollen",
    "Bestatigen und Buch offnen; Ansicht im Leser zurucksetzen", "Zuruck; Leser verlassen",
    "Im Regal zu Favoriten; Lesefortschritt anzeigen", "Favorit im Regal entfernen; Kapitelliste beim Lesen offnen",
    "Kategorien wechseln; PDF/EPUB zoomen", "PDF/EPUB nach links oder rechts drehen",
    "Einstellungsmenue offnen oder schliessen", "Start und Select zusammen halten zum Vollbeenden",
    "Lautstarke hoher oder niedriger", "Unterstuetze mich und kontaktiere mich fuer deinen exklusiven Avatar", "Beitrag",
}};

constexpr std::array<const char *, kTextCount> kTextJa = {{
    "ROCreader", u8"\u30b7\u30b9\u30c6\u30e0", u8"\u30ad\u30fc\u30ac\u30a4\u30c9",
    u8"\u30ad\u30fc\u6821\u6b63",
    u8"\u5c65\u6b74\u3092\u6d88\u53bb", u8"\u30ad\u30e3\u30c3\u30b7\u30e5\u3092\u6d88\u53bb", u8"\u30c6\u30ad\u30b9\u30c8\u8a2d\u5b9a",
    u8"\u8ca2\u732e\u8005", u8"\u9023\u7d61\u5148", u8"\u66f4\u65b0", u8"URL\u5165\u53e3",
    u8"\u7d42\u4e86",
    u8"A\u3067\u5b8c\u5168\u7d42\u4e86", u8"\u30ad\u30fc\u97f3", u8"\u660e\u308b\u3055",
    u8"\u81ea\u52d5\u30b9\u30ea\u30fc\u30d7", u8"\u30b9\u30ea\u30fc\u30d7\u9593\u9694",
    u8"\u30b7\u30b9\u30c6\u30e0\u8a00\u8a9e", u8"\u30aa\u30f3", u8"\u30aa\u30d5", u8"\u6d88\u53bb",
    u8"\u30ad\u30e3\u30c3\u30b7\u30e5\u3092\u6d88\u53bb", u8"\u5c65\u6b74\u3092\u6d88\u53bb",
    u8"\u80cc\u666f\u8272", u8"\u6587\u5b57\u8272", u8"\u6587\u5b57\u30b5\u30a4\u30ba",
    u8"TXT\u5909\u63db", u8"\u958b\u59cb", u8"\u73fe\u5728\u306e\u30d0\u30fc\u30b8\u30e7\u30f3",
    u8"\u66f4\u65b0\u3092\u78ba\u8a8d", u8"A\u3067\u66f4\u65b0\u3092\u78ba\u8a8d",
    u8"\u30cd\u30c3\u30c8\u63a5\u7d9a\u306a\u3057", u8"\u66f4\u65b0\u3092\u691c\u51fa\u3001\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u4e2d",
    u8"\u30d1\u30c3\u30b1\u30fc\u30b8\u3092\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u6e08\u307f",
    u8"\u518d\u8d77\u52d5\u3057\u3066\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb",
    u8"\u6700\u65b0\u7248\u3067\u3059", u8"\u30c0\u30a6\u30f3\u30ed\u30fc\u30c9\u5931\u6557\u3001\u5f8c\u3067\u518d\u8a66\u884c",
    u8"34XX SP\u5c02\u7528\u30de\u30c3\u30d4\u30f3\u30b0", u8"35XX H\u5c02\u7528\u30de\u30c3\u30d4\u30f3\u30b0", u8"H700\u6a19\u6e96\u30de\u30c3\u30d4\u30f3\u30b0", u8"Trimui Brick\u5c02\u7528\u30de\u30c3\u30d4\u30f3\u30b0",
    u8"\u672c\u68da\u3092\u79fb\u52d5\uff1b\u95b2\u89a7\u4e2d\u306f\u30da\u30fc\u30b8\u79fb\u52d5\u307e\u305f\u306f\u30b9\u30af\u30ed\u30fc\u30eb",
    u8"\u6c7a\u5b9a\u3057\u3066\u672c\u3092\u958b\u304f\uff1b\u30ea\u30fc\u30c0\u30fc\u3067\u8868\u793a\u3092\u30ea\u30bb\u30c3\u30c8",
    u8"\u623b\u308b\uff1b\u30ea\u30fc\u30c0\u30fc\u3092\u7d42\u4e86",
    u8"\u672c\u68da\u3067\u304a\u6c17\u306b\u5165\u308a\u8ffd\u52a0\uff1b\u8aad\u66f8\u9032\u6357\u3092\u8868\u793a",
    u8"\u672c\u68da\u306e\u304a\u6c17\u306b\u5165\u308a\u89e3\u9664\uff1b\u8aad\u66f8\u4e2d\u306b\u7ae0\u76ee\u6b21\u3092\u958b\u304f",
    u8"\u672c\u68da\u30ab\u30c6\u30b4\u30ea\u30fc\u5207\u66ff\uff1bPDF/EPUB\u30ba\u30fc\u30e0",
    u8"PDF/EPUB\u3092\u5de6\u53f3\u306b\u56de\u8ee2",
    u8"\u8a2d\u5b9a\u30e1\u30cb\u30e5\u30fc\u3092\u958b\u9589",
    u8"Start\u3068Select\u3092\u540c\u6642\u9577\u62bc\u3057\u3067\u5b8c\u5168\u7d42\u4e86",
    u8"\u97f3\u91cf\u3092\u4e0a\u3052\u308b / \u4e0b\u3052\u308b",
    u8"\u6253\u8d5e\u3057\u3066\u9023\u7d61\u3092\u304f\u3060\u3055\u3044\u3002\u3042\u306a\u305f\u5c02\u7528\u306e\u30a2\u30d0\u30bf\u30fc\u3092\u8ffd\u52a0\u3057\u307e\u3059",
    u8"\u8ca2\u732e\u5024",
}};

constexpr std::array<const char *, kTextCount> kTextKo = {{
    "ROCreader", u8"\uc2dc\uc2a4\ud15c", u8"\ud0a4 \uac00\uc774\ub4dc", u8"\ud0a4 \ubcf4\uc815", u8"\uae30\ub85d \uc9c0\uc6b0\uae30",
    u8"\uce90\uc2dc \uc9c0\uc6b0\uae30", u8"\ud14d\uc2a4\ud2b8 \uc124\uc815", u8"\uae30\uc5ec\uc790", u8"\uc5f0\ub77d",
    u8"\uc5c5\ub370\uc774\ud2b8", u8"URL \uc785\uad6c", u8"\uc885\ub8cc",
    u8"A\ub97c \ub20c\ub7ec \uc644\uc804 \uc885\ub8cc",
    u8"\ud0a4 \uc18c\ub9ac", u8"\ubc1d\uae30", u8"\uc790\ub3d9 \uc218\uba74", u8"\uc218\uba74 \uc2dc\uac04",
    u8"\uc2dc\uc2a4\ud15c \uc5b8\uc5b4", u8"\ucf1c\uae30", u8"\ub044\uae30", u8"\uc9c0\uc6b0\uae30",
    u8"\uce90\uc2dc \uc9c0\uc6b0\uae30", u8"\uae30\ub85d \uc9c0\uc6b0\uae30", u8"\ubc30\uacbd\uc0c9",
    u8"\uae00\uc790\uc0c9", u8"\uae00\uc790 \ud06c\uae30", "TXT", u8"\uc2dc\uc791", u8"\ud604\uc7ac \ubc84\uc804",
    u8"\uc5c5\ub370\uc774\ud2b8 \ud655\uc778", u8"A\ub97c \ub20c\ub7ec \ud655\uc778",
    u8"\ub124\ud2b8\uc6cc\ud06c \uc5f0\uacb0 \uc5c6\uc74c", u8"\uc5c5\ub370\uc774\ud2b8 \ubc1c\uacac, \ub2e4\uc6b4\ub85c\ub4dc \uc911",
    u8"\ud328\ud0a4\uc9c0 \ub2e4\uc6b4\ub85c\ub4dc \uc644\ub8cc", u8"\uc7ac\uc2dc\uc791\ud558\uc5ec \uc124\uce58",
    u8"\uc774\ubbf8 \ucd5c\uc2e0 \ubc84\uc804", u8"\ub2e4\uc6b4\ub85c\ub4dc \uc2e4\ud328, \ub2e4\uc2dc \uc2dc\ub3c4",
    u8"34XX SP \uc804\uc6a9 \ub9e4\ud551", u8"35XX H \uc804\uc6a9 \ub9e4\ud551", u8"H700 \uae30\ubcf8 \ub9e4\ud551", u8"Trimui Brick \uc804\uc6a9 \ub9e4\ud551",
    u8"\ucc45\uc7a5\uc744 \uc774\ub3d9\ud558\uace0 \uc77d\ub294 \uc911\uc5d0\ub294 \ud398\uc774\uc9c0\ub97c \ub118\uae30\uac70\ub098 \uc2a4\ud06c\ub864",
    u8"\ud655\uc778 \ubc0f \ucc45 \uc5f4\uae30; \ub9ac\ub354\uc5d0\uc11c \ud654\uba74 \ucd08\uae30\ud654",
    u8"\ub4a4\ub85c \uac00\uae30; \ub9ac\ub354 \uc885\ub8cc",
    u8"\ucc45\uc7a5\uc5d0\uc11c \uc990\uaca8\ucc3e\uae30 \ucd94\uac00; \uc77d\uae30 \uc9c4\ud589\ub960 \ud45c\uc2dc",
    u8"\ucc45\uc7a5\uc5d0\uc11c \uc990\uaca8\ucc3e\uae30 \ud574\uc81c; \uc77d\uae30 \uc911 \uc7a5 \ubaa9\ub85d \uc5f4\uae30",
    u8"\ucc45\uc7a5 \uce74\ud14c\uace0\ub9ac \uc804\ud658; PDF/EPUB \ud655\ub300/\ucd95\uc18c",
    u8"PDF/EPUB \uc67c\ucabd / \uc624\ub978\ucabd \ud68c\uc804",
    u8"\uc124\uc815 \uba54\ub274 \uc5f4\uae30 / \ub2eb\uae30",
    u8"Start\uc640 Select\ub97c \ud568\uaed8 \uae38\uac8c \ub20c\ub7ec \uc644\uc804 \uc885\ub8cc",
    u8"\uc74c\ub7c9 \uc62c\ub9ac\uae30 / \ub0ae\ucd94\uae30",
    u8"\ud6c4\uc6d0\ud558\uc2dc\uace0 \uc5f0\ub77d\ud574 \uc8fc\uc2dc\uba74 \uc804\uc6a9 \uc544\ubc14\ud0c0\ub97c \ucd94\uac00\ud574 \ub4dc\ub9bd\ub2c8\ub2e4",
    u8"\uae30\uc5ec\uac12",
}};

constexpr std::array<const char *, kTextCount> kTextAr = {{
    "ROCreader", u8"\u0627\u0644\u0646\u0638\u0627\u0645", u8"\u062f\u0644\u064a\u0644 \u0627\u0644\u0623\u0632\u0631\u0627\u0631",
    u8"\u0645\u0639\u0627\u064a\u0631\u0629",
    u8"\u0645\u0633\u062d \u0627\u0644\u0633\u062c\u0644", u8"\u0645\u0633\u062d \u0627\u0644\u0630\u0627\u0643\u0631\u0629", u8"\u0625\u0639\u062f\u0627\u062f\u0627\u062a \u0627\u0644\u0646\u0635",
    u8"\u0627\u0644\u0645\u0633\u0627\u0647\u0645\u0648\u0646", u8"\u0627\u062a\u0635\u0644 \u0628\u064a",
    u8"\u0627\u0644\u062a\u062d\u062f\u064a\u062b\u0627\u062a", u8"\u0645\u062f\u062e\u0644 URL",
    u8"\u062e\u0631\u0648\u062c",
    u8"\u0627\u0636\u063a\u0637 A \u0644\u0644\u062e\u0631\u0648\u062c \u0627\u0644\u0643\u0627\u0645\u0644",
    u8"\u0635\u0648\u062a \u0627\u0644\u0623\u0632\u0631\u0627\u0631", u8"\u0627\u0644\u0633\u0637\u0648\u0639",
    u8"\u0627\u0644\u0633\u0643\u0648\u0646 \u0627\u0644\u062a\u0644\u0642\u0627\u0626\u064a",
    u8"\u0645\u0624\u0642\u062a \u0627\u0644\u0646\u0648\u0645", u8"\u0644\u063a\u0629 \u0627\u0644\u0646\u0638\u0627\u0645",
    u8"\u062a\u0634\u063a\u064a\u0644", u8"\u0625\u064a\u0642\u0627\u0641", u8"\u0645\u0633\u062d",
    u8"\u0645\u0633\u062d \u0627\u0644\u0630\u0627\u0643\u0631\u0629", u8"\u0645\u0633\u062d \u0627\u0644\u0633\u062c\u0644",
    u8"\u0644\u0648\u0646 \u0627\u0644\u062e\u0644\u0641\u064a\u0629", u8"\u0644\u0648\u0646 \u0627\u0644\u062e\u0637",
    u8"\u062d\u062c\u0645 \u0627\u0644\u062e\u0637", "TXT", u8"\u0627\u0628\u062f\u0623",
    u8"\u0627\u0644\u0625\u0635\u062f\u0627\u0631 \u0627\u0644\u062d\u0627\u0644\u064a",
    u8"\u0627\u0644\u062a\u062d\u0642\u0642 \u0645\u0646 \u0627\u0644\u062a\u062d\u062f\u064a\u062b\u0627\u062a",
    u8"\u0627\u0636\u063a\u0637 A \u0644\u0644\u062a\u062d\u0642\u0642", u8"\u0644\u0627 \u064a\u0648\u062c\u062f \u0627\u062a\u0635\u0627\u0644 \u0634\u0628\u0643\u064a",
    u8"\u062a\u0645 \u0627\u0644\u0639\u062b\u0648\u0631 \u0639\u0644\u0649 \u062a\u062d\u062f\u064a\u062b\u060c \u062c\u0627\u0631\u064d \u0627\u0644\u062a\u0646\u0632\u064a\u0644",
    u8"\u062a\u0645 \u062a\u0646\u0632\u064a\u0644 \u0627\u0644\u062d\u0632\u0645\u0629", u8"\u0623\u0639\u062f \u0627\u0644\u062a\u0634\u063a\u064a\u0644 \u0644\u0644\u062a\u062b\u0628\u064a\u062a",
    u8"\u0623\u0646\u062a \u0639\u0644\u0649 \u0623\u062d\u062f\u062b \u0625\u0635\u062f\u0627\u0631",
    u8"\u0641\u0634\u0644 \u0627\u0644\u062a\u0646\u0632\u064a\u0644\u060c \u062d\u0627\u0648\u0644 \u0644\u0627\u062d\u0642\u0627",
    u8"\u062a\u062e\u0637\u064a\u0637 34XX SP", u8"\u062a\u062e\u0637\u064a\u0637 35XX H", u8"\u062a\u062e\u0637\u064a\u0637 H700 \u0627\u0644\u0639\u0627\u0645", u8"\u062a\u062e\u0637\u064a\u0637 Trimui Brick",
    u8"\u062a\u0635\u0641\u062d \u0627\u0644\u0631\u0641\u060c \u0648\u0627\u0644\u062a\u0642\u0644\u0628 \u0623\u0648 \u0627\u0644\u062a\u0645\u0631\u064a\u0631 \u0623\u062b\u0646\u0627\u0621 \u0627\u0644\u0642\u0631\u0627\u0621\u0629",
    u8"\u062a\u0623\u0643\u064a\u062f \u0648\u0641\u062a\u062d \u0627\u0644\u0643\u062a\u0628\u061b \u0625\u0639\u0627\u062f\u0629 \u0636\u0628\u0637 \u0627\u0644\u0639\u0631\u0636 \u0641\u064a \u0627\u0644\u0642\u0627\u0631\u0626",
    u8"\u0631\u062c\u0648\u0639\u061b \u0627\u0644\u062e\u0631\u0648\u062c \u0645\u0646 \u0627\u0644\u0642\u0627\u0631\u0626",
    u8"\u0625\u0636\u0627\u0641\u0629 \u0645\u0641\u0636\u0644\u0629 \u0645\u0646 \u0627\u0644\u0631\u0641\u061b \u0639\u0631\u0636 \u062a\u0642\u062f\u0645 \u0627\u0644\u0642\u0631\u0627\u0621\u0629",
    u8"\u0625\u0632\u0627\u0644\u0629 \u0627\u0644\u0645\u0641\u0636\u0644\u0629 \u0645\u0646 \u0627\u0644\u0631\u0641\u061b \u0641\u062a\u062d \u0641\u0647\u0631\u0633 \u0627\u0644\u0641\u0635\u0648\u0644 \u0623\u062b\u0646\u0627\u0621 \u0627\u0644\u0642\u0631\u0627\u0621\u0629",
    u8"\u062a\u0628\u062f\u064a\u0644 \u0641\u0626\u0627\u062a \u0627\u0644\u0631\u0641\u061b \u062a\u0643\u0628\u064a\u0631 PDF/EPUB",
    u8"\u062a\u062f\u0648\u064a\u0631 PDF/EPUB \u064a\u0633\u0627\u0631\u0627\u064b / \u064a\u0645\u064a\u0646\u0627\u064b",
    u8"\u0641\u062a\u062d \u0623\u0648 \u0625\u063a\u0644\u0627\u0642 \u0642\u0627\u0626\u0645\u0629 \u0627\u0644\u0625\u0639\u062f\u0627\u062f\u0627\u062a",
    u8"\u0627\u0636\u063a\u0637 Start \u0648 Select \u0645\u0639\u0627\u064b \u0644\u0644\u062e\u0631\u0648\u062c \u0627\u0644\u0643\u0627\u0645\u0644",
    u8"\u0631\u0641\u0639 / \u062e\u0641\u0636 \u0627\u0644\u0635\u0648\u062a",
    u8"\u0627\u062f\u0639\u0645\u0646\u064a \u0648\u062a\u0648\u0627\u0635\u0644 \u0645\u0639\u064a \u0644\u0625\u0636\u0627\u0641\u0629 \u0635\u0648\u0631\u062a\u0643 \u0627\u0644\u062d\u0635\u0631\u064a\u0629",
    u8"\u0642\u064a\u0645\u0629 \u0627\u0644\u0645\u0633\u0627\u0647\u0645\u0629",
}};

constexpr std::array<const char *, kTextCount> kTextRu = {{
    "ROCreader", u8"\u0421\u0438\u0441\u0442\u0435\u043c\u0430", u8"\u041a\u043d\u043e\u043f\u043a\u0438",
    u8"\u041a\u0430\u043b\u0438\u0431\u0440.",
    u8"\u041e\u0447\u0438\u0441\u0442\u0438\u0442\u044c \u0438\u0441\u0442\u043e\u0440\u0438\u044e",
    u8"\u041e\u0447\u0438\u0441\u0442\u0438\u0442\u044c \u043a\u044d\u0448", u8"\u0422\u0435\u043a\u0441\u0442", u8"\u0410\u0432\u0442\u043e\u0440\u044b",
    u8"\u041a\u043e\u043d\u0442\u0430\u043a\u0442", u8"\u041e\u0431\u043d\u043e\u0432\u043b\u0435\u043d\u0438\u044f",
    u8"URL \u0432\u0445\u043e\u0434",
    u8"\u0412\u044b\u0445\u043e\u0434", u8"\u041d\u0430\u0436\u043c\u0438\u0442\u0435 A \u0434\u043b\u044f \u0432\u044b\u0445\u043e\u0434\u0430",
    u8"\u0417\u0432\u0443\u043a \u043a\u043b\u0430\u0432\u0438\u0448", u8"\u042f\u0440\u043a\u043e\u0441\u0442\u044c",
    u8"\u0410\u0432\u0442\u043e\u0441\u043e\u043d",
    u8"\u0422\u0430\u0439\u043c\u0435\u0440 \u0441\u043d\u0430", u8"\u042f\u0437\u044b\u043a \u0441\u0438\u0441\u0442\u0435\u043c\u044b",
    u8"\u0412\u043a\u043b", u8"\u0412\u044b\u043a\u043b", u8"\u041e\u0447\u0438\u0441\u0442\u0438\u0442\u044c",
    u8"\u041e\u0447\u0438\u0441\u0442\u0438\u0442\u044c \u043a\u044d\u0448", u8"\u041e\u0447\u0438\u0441\u0442\u0438\u0442\u044c \u0438\u0441\u0442\u043e\u0440\u0438\u044e",
    u8"\u0424\u043e\u043d", u8"\u0426\u0432\u0435\u0442 \u0448\u0440\u0438\u0444\u0442\u0430",
    u8"\u0420\u0430\u0437\u043c\u0435\u0440 \u0448\u0440\u0438\u0444\u0442\u0430", "TXT", u8"\u0421\u0442\u0430\u0440\u0442",
    u8"\u0422\u0435\u043a\u0443\u0449\u0430\u044f \u0432\u0435\u0440\u0441\u0438\u044f",
    u8"\u041f\u0440\u043e\u0432\u0435\u0440\u0438\u0442\u044c \u043e\u0431\u043d\u043e\u0432\u043b\u0435\u043d\u0438\u044f",
    u8"\u041d\u0430\u0436\u043c\u0438\u0442\u0435 A \u0434\u043b\u044f \u043f\u0440\u043e\u0432\u0435\u0440\u043a\u0438",
    u8"\u041d\u0435\u0442 \u0441\u0435\u0442\u0438", u8"\u041d\u0430\u0439\u0434\u0435\u043d\u043e \u043e\u0431\u043d\u043e\u0432\u043b\u0435\u043d\u0438\u0435, \u0438\u0434\u0435\u0442 \u0437\u0430\u0433\u0440\u0443\u0437\u043a\u0430",
    u8"\u041f\u0430\u043a\u0435\u0442 \u0437\u0430\u0433\u0440\u0443\u0436\u0435\u043d", u8"\u041f\u0435\u0440\u0435\u0437\u0430\u043f\u0443\u0441\u0442\u0438\u0442\u0435 \u0434\u043b\u044f \u0443\u0441\u0442\u0430\u043d\u043e\u0432\u043a\u0438",
    u8"\u0423\u0436\u0435 \u0430\u043a\u0442\u0443\u0430\u043b\u044c\u043d\u043e",
    u8"\u0417\u0430\u0433\u0440\u0443\u0437\u043a\u0430 \u043d\u0435 \u0443\u0434\u0430\u043b\u0430\u0441\u044c, \u043f\u043e\u0432\u0442\u043e\u0440\u0438\u0442\u0435",
    u8"\u0420\u0430\u0441\u043a\u043b\u0430\u0434\u043a\u0430 34XX SP", u8"\u0420\u0430\u0441\u043a\u043b\u0430\u0434\u043a\u0430 35XX H", u8"\u0421\u0442\u0430\u043d\u0434\u0430\u0440\u0442\u043d\u0430\u044f \u0440\u0430\u0441\u043a\u043b\u0430\u0434\u043a\u0430 H700", u8"\u0420\u0430\u0441\u043a\u043b\u0430\u0434\u043a\u0430 Trimui Brick",
    u8"\u041f\u0435\u0440\u0435\u043c\u0435\u0449\u0435\u043d\u0438\u0435 \u043f\u043e \u043f\u043e\u043b\u043a\u0435; \u043f\u0435\u0440\u0435\u043b\u0438\u0441\u0442\u044b\u0432\u0430\u043d\u0438\u0435 \u0438\u043b\u0438 \u043f\u0440\u043e\u043a\u0440\u0443\u0442\u043a\u0430 \u043f\u0440\u0438 \u0447\u0442\u0435\u043d\u0438\u0438",
    u8"\u041f\u043e\u0434\u0442\u0432\u0435\u0440\u0434\u0438\u0442\u044c \u0438 \u043e\u0442\u043a\u0440\u044b\u0442\u044c \u043a\u043d\u0438\u0433\u0443; \u0441\u0431\u0440\u043e\u0441 \u0432\u0438\u0434\u0430 \u0432 \u0447\u0438\u0442\u0430\u043b\u043a\u0435",
    u8"\u041d\u0430\u0437\u0430\u0434; \u0432\u044b\u0445\u043e\u0434 \u0438\u0437 \u0447\u0438\u0442\u0430\u043b\u043a\u0438",
    u8"\u0414\u043e\u0431\u0430\u0432\u0438\u0442\u044c \u0432 \u0438\u0437\u0431\u0440\u0430\u043d\u043d\u043e\u0435; \u043f\u043e\u043a\u0430\u0437\u0430\u0442\u044c \u043f\u0440\u043e\u0433\u0440\u0435\u0441\u0441 \u0447\u0442\u0435\u043d\u0438\u044f",
    u8"\u0423\u0431\u0440\u0430\u0442\u044c \u0438\u0437 \u0438\u0437\u0431\u0440\u0430\u043d\u043d\u043e\u0433\u043e; \u043e\u0442\u043a\u0440\u044b\u0442\u044c \u0441\u043f\u0438\u0441\u043e\u043a \u0433\u043b\u0430\u0432 \u043f\u0440\u0438 \u0447\u0442\u0435\u043d\u0438\u0438",
    u8"\u041f\u0435\u0440\u0435\u043a\u043b\u044e\u0447\u0435\u043d\u0438\u0435 \u043a\u0430\u0442\u0435\u0433\u043e\u0440\u0438\u0439; \u043c\u0430\u0441\u0448\u0442\u0430\u0431 PDF/EPUB",
    u8"\u041f\u043e\u0432\u043e\u0440\u043e\u0442 PDF/EPUB \u0432\u043b\u0435\u0432\u043e / \u0432\u043f\u0440\u0430\u0432\u043e",
    u8"\u041e\u0442\u043a\u0440\u044b\u0442\u044c \u0438\u043b\u0438 \u0437\u0430\u043a\u0440\u044b\u0442\u044c \u043c\u0435\u043d\u044e \u043d\u0430\u0441\u0442\u0440\u043e\u0435\u043a",
    u8"\u0423\u0434\u0435\u0440\u0436\u0438\u0432\u0430\u0439\u0442\u0435 Start \u0438 Select \u0432\u043c\u0435\u0441\u0442\u0435 \u0434\u043b\u044f \u043f\u043e\u043b\u043d\u043e\u0433\u043e \u0432\u044b\u0445\u043e\u0434\u0430",
    u8"\u0423\u0432\u0435\u043b\u0438\u0447\u0438\u0442\u044c / \u0443\u043c\u0435\u043d\u044c\u0448\u0438\u0442\u044c \u0433\u0440\u043e\u043c\u043a\u043e\u0441\u0442\u044c",
    u8"\u041f\u043e\u0434\u0434\u0435\u0440\u0436\u0438\u0442\u0435 \u043c\u0435\u043d\u044f \u0438 \u0441\u0432\u044f\u0436\u0438\u0442\u0435\u0441\u044c \u0441\u043e \u043c\u043d\u043e\u0439, \u0447\u0442\u043e\u0431\u044b \u0434\u043e\u0431\u0430\u0432\u0438\u0442\u044c \u0432\u0430\u0448 \u043b\u0438\u0447\u043d\u044b\u0439 \u0430\u0432\u0430\u0442\u0430\u0440",
    u8"\u0412\u043a\u043b\u0430\u0434",
}};

constexpr std::array<const char *, kTextCount> kTextPt = {{
    "ROCreader", "Sistema", "Guia teclas", "Calibrar", "Limpar historico", "Limpar cache", "Texto", "Colaboradores",
    "Contato", "Atualizacoes", "Entrada URL", "Sair", "Pressione A para sair", "Som teclas", "Brilho",
    "Sono automatico", "Timer de sono", "Idioma do sistema", "Ligado", "Desligado", "Limpar",
    "Limpar cache", "Limpar historico", "Fundo", "Cor da fonte", "Tamanho da fonte", "TXT",
    "Iniciar", "Versao atual", "Verificar atualizacoes", "Pressione A para verificar",
    "Sem conexao de rede", "Atualizacao encontrada, baixando", "Pacote baixado",
    "Reinicie para instalar", "Ja esta atualizado", "Falha no download, tente depois",
    "Mapa 34XX SP", "Mapa 35XX H", "Mapa padrao H700", "Mapa Trimui Brick",
    "Navegar pela estante; virar pagina ou rolar durante a leitura",
    "Confirmar e abrir livros; redefinir a visualizacao no leitor", "Voltar; sair do leitor",
    "Adicionar favorito na estante; mostrar progresso de leitura", "Remover favorito da estante; abrir lista de capitulos durante a leitura",
    "Trocar categorias da estante; zoom em PDF/EPUB", "Girar PDF/EPUB para a esquerda ou direita",
    "Abrir ou fechar o menu de configuracoes", "Segure Start e Select juntos para sair completamente",
    "Aumentar ou diminuir o volume", "Apoie e entre em contato para adicionar seu avatar exclusivo",
    "Contribuicao",
}};

constexpr std::array<const char *, kTextCount> kTextVi = {{
    "ROCreader", u8"H\u1ec7 th\u1ed1ng", u8"H\u01b0\u1edbng d\u1eabn ph\u00edm",
    u8"Canh ph\u00edm",
    u8"X\u00f3a l\u1ecbch s\u1eed", u8"X\u00f3a b\u1ed9 nh\u1edb \u0111\u1ec7m", u8"C\u00e0i \u0111\u1eb7t v\u0103n b\u1ea3n",
    u8"C\u1ed9ng t\u00e1c vi\u00ean", u8"Li\u00ean h\u1ec7", u8"C\u1eadp nh\u1eadt", u8"M\u1ee5c URL", u8"Tho\u00e1t",
    u8"Nh\u1ea5n A \u0111\u1ec3 tho\u00e1t", u8"\u00c2m ph\u00edm", u8"\u0110\u1ed9 s\u00e1ng",
    u8"Ng\u1ee7 t\u1ef1 \u0111\u1ed9ng", u8"H\u1eb9n gi\u1edd ng\u1ee7", u8"Ng\u00f4n ng\u1eef h\u1ec7 th\u1ed1ng",
    u8"B\u1eadt", u8"T\u1eaft", u8"X\u00f3a", u8"X\u00f3a b\u1ed9 nh\u1edb \u0111\u1ec7m",
    u8"X\u00f3a l\u1ecbch s\u1eed", u8"M\u00e0u n\u1ec1n", u8"M\u00e0u ch\u1eef",
    u8"C\u1ee1 ch\u1eef", "TXT", u8"B\u1eaft \u0111\u1ea7u", u8"Phi\u00ean b\u1ea3n hi\u1ec7n t\u1ea1i",
    u8"Ki\u1ec3m tra c\u1eadp nh\u1eadt", u8"Nh\u1ea5n A \u0111\u1ec3 ki\u1ec3m tra",
    u8"Kh\u00f4ng c\u00f3 k\u1ebft n\u1ed1i m\u1ea1ng", u8"\u0110\u00e3 t\u00ecm th\u1ea5y b\u1ea3n c\u1eadp nh\u1eadt, \u0111ang t\u1ea3i",
    u8"\u0110\u00e3 t\u1ea3i g\u00f3i c\u00e0i \u0111\u1eb7t", u8"Kh\u1edfi \u0111\u1ed9ng l\u1ea1i \u0111\u1ec3 c\u00e0i \u0111\u1eb7t",
    u8"\u0110\u00e3 l\u00e0 b\u1ea3n m\u1edbi nh\u1ea5t", u8"T\u1ea3i xu\u1ed1ng th\u1ea5t b\u1ea1i, h\u00e3y th\u1eed l\u1ea1i",
    u8"S\u01a1 \u0111\u1ed3 34XX SP", u8"S\u01a1 \u0111\u1ed3 35XX H", u8"S\u01a1 \u0111\u1ed3 H700 ti\u00eau chu\u1ea9n", u8"S\u01a1 \u0111\u1ed3 Trimui Brick",
    u8"Duy\u1ec7t gi\u00e1 s\u00e1ch; l\u1eadt trang ho\u1eb7c cu\u1ed9n khi \u0111ang \u0111\u1ecdc",
    u8"X\u00e1c nh\u1eadn v\u00e0 m\u1edf s\u00e1ch; \u0111\u1eb7t l\u1ea1i khung nh\u00ecn trong tr\u00ecnh \u0111\u1ecdc",
    u8"Quay l\u1ea1i; tho\u00e1t kh\u1ecfi tr\u00ecnh \u0111\u1ecdc",
    u8"Th\u00eam y\u00eau th\u00edch trong gi\u00e1 s\u00e1ch; hi\u1ec3n th\u1ecb ti\u1ebfn \u0111\u1ed9 \u0111\u1ecdc",
    u8"B\u1ecf y\u00eau th\u00edch kh\u1ecfi gi\u00e1 s\u00e1ch; m\u1edf m\u1ee5c l\u1ee5c ch\u01b0\u01a1ng khi \u0111\u1ecdc",
    u8"Chuy\u1ec3n danh m\u1ee5c gi\u00e1 s\u00e1ch; ph\u00f3ng to PDF/EPUB",
    u8"Xoay PDF/EPUB sang tr\u00e1i / ph\u1ea3i",
    u8"M\u1edf ho\u1eb7c \u0111\u00f3ng menu c\u00e0i \u0111\u1eb7t",
    u8"Gi\u1eef Start v\u00e0 Select c\u00f9ng l\u00fac \u0111\u1ec3 tho\u00e1t ho\u00e0n to\u00e0n",
    u8"T\u0103ng / gi\u1ea3m \u00e2m l\u01b0\u1ee3ng",
    u8"\u1ee6ng h\u1ed9 v\u00e0 li\u00ean h\u1ec7 \u0111\u1ec3 th\u00eam avatar \u0111\u1ed9c quy\u1ec1n c\u1ee7a b\u1ea1n",
    u8"\u0110i\u1ec3m \u0111\u00f3ng g\u00f3p",
}};

constexpr int kSleepIntervalCount = 6;
constexpr std::array<const char *, kSleepIntervalCount> kSleepZh = {
    u8"30\u79d2", u8"1\u5206\u949f", u8"3\u5206\u949f", u8"5\u5206\u949f",
    u8"10\u5206\u949f", u8"30\u5206\u949f"};
constexpr std::array<const char *, kSleepIntervalCount> kSleepZhHant = {
    u8"30\u79d2", u8"1\u5206\u9418", u8"3\u5206\u9418", u8"5\u5206\u9418",
    u8"10\u5206\u9418", u8"30\u5206\u9418"};
constexpr std::array<const char *, kSleepIntervalCount> kSleepEn = {
    "30 sec", "1 min", "3 min", "5 min", "10 min", "30 min"};

std::string LowerAscii(std::string value) {
  for (char &ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

const std::array<const char *, kTextCount> &LanguageTexts(int language_index) {
  switch (ClampSystemLanguageIndex(language_index)) {
  case 0: return kTextZh;
  case 1: return kTextZhHant;
  case 2: return kTextEn;
  case 3: return kTextEs;
  case 4: return kTextFr;
  case 5: return kTextDe;
  case 6: return kTextJa;
  case 7: return kTextKo;
  case 8: return kTextAr;
  case 9: return kTextRu;
  case 10: return kTextPt;
  case 11: return kTextVi;
  default: return kTextEn;
  }
}

const std::array<const char *, kSleepIntervalCount> &SleepTexts(int language_index) {
  switch (ClampSystemLanguageIndex(language_index)) {
  case 0: return kSleepZh;
  case 1: return kSleepZhHant;
  default: return kSleepEn;
  }
}

void ReplaceAll(std::string &text, const std::string &from, const std::string &to) {
  if (from.empty()) return;
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

const char *DownloadSpeedPrefix(int language_index) {
  switch (ClampSystemLanguageIndex(language_index)) {
  case 0: return u8"\u4e0b\u8f7d\u901f\u5ea6";
  case 1: return u8"\u4e0b\u8f09\u901f\u5ea6";
  case 3: return "Velocidad";
  case 4: return "Vitesse";
  case 5: return "Tempo";
  case 6: return u8"\u901f\u5ea6";
  case 7: return u8"\uc18d\ub3c4";
  case 8: return u8"\u0633\u0631\u0639\u0629 \u0627\u0644\u062a\u0646\u0632\u064a\u0644";
  case 9: return u8"\u0421\u043a\u043e\u0440\u043e\u0441\u0442\u044c";
  case 10: return "Velocidade";
  case 11: return u8"T\u1ed1c \u0111\u1ed9";
  default: return "Download speed";
  }
}
} // namespace

int ClampSystemLanguageIndex(int value) {
  return std::clamp(value, 0, static_cast<int>(kLanguages.size()) - 1);
}

int SystemLanguageCount() {
  return static_cast<int>(kLanguages.size());
}

const char *SystemLanguageDisplayLabel(int index) {
  return kLanguages[static_cast<size_t>(ClampSystemLanguageIndex(index))].display_label;
}

const char *SystemLanguageConfigValue(int index) {
  return kLanguages[static_cast<size_t>(ClampSystemLanguageIndex(index))].config_value;
}

std::string NormalizeSystemLanguageConfigValue(const std::string &value) {
  const std::string lowered = LowerAscii(value);
  if (lowered == "zh" || lowered == "zh-cn" || lowered == "zh_cn" || lowered == "zh-hans" || lowered == "zh_hans") {
    return "zh";
  }
  if (lowered == "zh-hant" || lowered == "zh_hant" || lowered == "zh-tw" || lowered == "zh_tw") {
    return "zh-Hant";
  }
  if (lowered == "en") return "en";
  if (lowered == "es") return "es";
  if (lowered == "fr") return "fr";
  if (lowered == "de") return "de";
  if (lowered == "ja") return "ja";
  if (lowered == "ko") return "ko";
  if (lowered == "ar") return "ar";
  if (lowered == "ru") return "ru";
  if (lowered == "pt" || lowered == "pt-br" || lowered == "pt_br" || lowered == "pt-pt" || lowered == "pt_pt") {
    return "pt";
  }
  if (lowered == "vi") return "vi";
  return "zh";
}

int SystemLanguageIndexFromConfigValue(const std::string &value) {
  const std::string normalized = NormalizeSystemLanguageConfigValue(value);
  for (size_t i = 0; i < kLanguages.size(); ++i) {
    if (normalized == kLanguages[i].config_value) return static_cast<int>(i);
  }
  return 0;
}

const char *LocalizedAppText(int language_index, AppTextId text_id) {
  return LanguageTexts(language_index)[static_cast<size_t>(text_id)];
}

const char *LocalizedSleepIntervalLabel(int language_index, int interval_index) {
  return SleepTexts(language_index)[static_cast<size_t>(std::clamp(interval_index, 0, kSleepIntervalCount - 1))];
}

std::string LocalizedDownloadSpeedText(int language_index, double bytes_per_sec) {
  constexpr double kKiB = 1024.0;
  constexpr double kMiB = 1024.0 * 1024.0;
  std::ostringstream oss;
  oss.setf(std::ios::fixed);
  oss << DownloadSpeedPrefix(language_index) << " ";
  if (bytes_per_sec <= 0.0) {
    oss.precision(1);
    oss << "0.0 KB/s";
  } else if (bytes_per_sec >= kMiB) {
    oss.precision(2);
    oss << (bytes_per_sec / kMiB) << " MB/s";
  } else {
    oss.precision(1);
    oss << (bytes_per_sec / kKiB) << " KB/s";
  }
  return oss.str();
}

std::string LocalizedBootScanText(int language_index, size_t current, size_t total) {
  std::ostringstream oss;
  if (ClampSystemLanguageIndex(language_index) == 1) {
    oss << u8"\u8cc7\u6e90\u8f09\u5165\u4e2d...(";
  } else if (ClampSystemLanguageIndex(language_index) == 0) {
    oss << u8"\u8d44\u6e90\u52a0\u8f7d\u4e2d...(";
  } else {
    oss << "Loading resources...(";
  }
  oss << current << "/" << total << ")";
  return oss.str();
}

std::string LocalizedBootCoverText(int language_index, size_t current, size_t total) {
  std::ostringstream oss;
  if (ClampSystemLanguageIndex(language_index) == 1) {
    oss << u8"\u5c01\u9762\u5feb\u53d6\u751f\u6210\u4e2d...(";
  } else if (ClampSystemLanguageIndex(language_index) == 0) {
    oss << u8"\u5c01\u9762\u7f13\u5b58\u751f\u6210\u4e2d...(";
  } else {
    oss << "Building cover cache...(";
  }
  oss << current << "/" << total << ")";
  return oss.str();
}

std::string LocalizedUpdateReplayText(int language_index, float ratio, bool success, const std::string &version) {
  const std::string version_suffix = version.empty() ? std::string() : (" " + version);
  if (!success) {
    if (ClampSystemLanguageIndex(language_index) == 1) {
      return u8"\u66f4\u65b0\u5b89\u88dd\u672a\u5b8c\u6210\uff0c\u6b63\u5728\u555f\u52d5\u76ee\u524d\u7248\u672c";
    }
    if (ClampSystemLanguageIndex(language_index) == 0) {
      return u8"\u66f4\u65b0\u5b89\u88c5\u672a\u5b8c\u6210\uff0c\u6b63\u5728\u542f\u52a8\u5f53\u524d\u7248\u672c";
    }
    return "Update install incomplete, starting current version";
  }
  if (ratio < 0.30f) {
    if (ClampSystemLanguageIndex(language_index) == 1) return std::string(u8"\u6b63\u5728\u5b89\u88dd\u66f4\u65b0\u5957\u4ef6") + version_suffix;
    if (ClampSystemLanguageIndex(language_index) == 0) return std::string(u8"\u6b63\u5728\u5b89\u88c5\u66f4\u65b0\u5305") + version_suffix;
    return "Installing update package" + version_suffix;
  }
  if (ratio < 0.60f) {
    if (ClampSystemLanguageIndex(language_index) == 1) return std::string(u8"\u6b63\u5728\u89e3\u58d3\u66f4\u65b0\u8cc7\u6e90") + version_suffix;
    if (ClampSystemLanguageIndex(language_index) == 0) return std::string(u8"\u6b63\u5728\u89e3\u538b\u66f4\u65b0\u8d44\u6e90") + version_suffix;
    return "Extracting update resources" + version_suffix;
  }
  if (ratio < 0.90f) {
    if (ClampSystemLanguageIndex(language_index) == 1) return std::string(u8"\u6b63\u5728\u66ff\u63db\u57f7\u884c\u6642\u6a94\u6848") + version_suffix;
    if (ClampSystemLanguageIndex(language_index) == 0) return std::string(u8"\u6b63\u5728\u66ff\u6362\u8fd0\u884c\u65f6\u6587\u4ef6") + version_suffix;
    return "Replacing runtime files" + version_suffix;
  }
  if (ClampSystemLanguageIndex(language_index) == 1) {
    return u8"\u66f4\u65b0\u5b89\u88dd\u5b8c\u6210\uff0c\u5373\u5c07\u9032\u5165\u66f8\u67b6";
  }
  if (ClampSystemLanguageIndex(language_index) == 0) {
    return u8"\u66f4\u65b0\u5b89\u88c5\u5b8c\u6210\uff0c\u5373\u5c06\u8fdb\u5165\u4e66\u67b6";
  }
  return "Update installed, entering shelf";
}

std::string LocalizeContributionLabel(int language_index, const std::string &label) {
  std::string localized = label;
  ReplaceAll(localized, u8"\u8d21\u732e\u503c", LocalizedAppText(language_index, AppTextId::ContributionValue));
  return localized;
}
