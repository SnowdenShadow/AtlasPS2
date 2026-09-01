# Installer AtlasPS2 sur PS2

Ce guide est écrit pour quelqu'un qui n'a jamais fait ça. Chaque étape dit
exactement quoi copier, sur quel bouton appuyer, et ce qui doit s'afficher
à l'écran. Si ce que vous voyez ne correspond pas à ce qui est écrit,
arrêtez-vous et allez voir **En cas de problème** à la fin.

Comptez vingt minutes la première fois.

---

## Ce qu'il faut

* **Une PlayStation 2**, n'importe quel modèle, branchée sur une télévision.
* **Une Memory Card officielle Sony de 8 Mo**, celle sur laquelle AtlasPS2
  sera installé. Les cartes de contrefaçon « 64 Mo » ou « 128 Mo » ne
  fonctionnent pas de façon fiable pour démarrer et ne sont pas
  supportées.
* **Une clé USB**, formatée en **FAT32**. Une clé de 2 Go suffit
  largement ; les très grosses clés sont parfois mal reconnues par la PS2.
* **Un moyen de lancer un homebrew** que vous avez *déjà* : FreeMcBoot,
  uLaunchELF, un Swap Magic, un modchip, ou une carte déjà préparée par
  quelqu'un.
* **Une manette** branchée dans le port 1.

> **AtlasPS2 n'ouvre pas la porte de votre console.** Il ne peut pas
> transformer une PS2 complètement d'origine en console qui lance des
> homebrews. Il faut que ce soit déjà le cas. Si vous n'avez aucun moyen
> de lancer un fichier `.ELF`, ce guide ne s'applique pas encore à vous :
> il faut d'abord installer un « bootstrap » (FreeMcBoot ou équivalent),
> ce qui est une opération séparée, avec ses propres guides, et qui
> dépend du modèle exact de votre console.

### Ce que l'installation ne détruit pas

L'installateur **fait une sauvegarde avant de toucher à quoi que ce soit**
et ne supprime jamais les dossiers `BOOT`, `APPS` et `SYS-CONF` en bloc.
Vos sauvegardes de jeux ne sont pas touchées : elles sont dans d'autres
dossiers, et rien ici ne les lit ni ne les écrit.

Cela dit, une Memory Card est un objet fragile qui a vingt ans. Si elle
contient des sauvegardes auxquelles vous tenez, copiez-les sur un PC
avant de commencer, avec uLaunchELF ou un adaptateur.

---

## Étape 1 — Préparer la clé USB

1. Branchez la clé USB sur votre ordinateur.
2. Si elle n'est pas en FAT32, formatez-la en FAT32. Sous Windows :
   clic droit sur la clé → **Formater** → système de fichiers **FAT32**
   → **Démarrer**. Cela efface la clé.
3. Ouvrez l'archive `AtlasPS2-v0.1.0.zip`.
4. Copiez ces deux fichiers **à la racine** de la clé, c'est-à-dire
   directement dessus, pas dans un dossier :

   * `ATLAS_INSTALLER.ELF`
   * `ATLASPS2.ELF`

5. Copiez aussi le dossier `ATLAS` de l'archive à la racine de la clé.

La clé doit ressembler à ceci :

```
E:\
├── ATLAS_INSTALLER.ELF
├── ATLASPS2.ELF
└── ATLAS\
    ├── LANG\
    └── THEMES\
```

`E:` est juste un exemple, chez vous ce sera peut-être `D:` ou `F:`.

6. Éjectez proprement la clé (sous Windows : clic sur l'icône
   « Retirer le périphérique en toute sécurité »). Une clé arrachée
   pendant une écriture peut arriver sur la PS2 avec des fichiers
   incomplets.

> **Les majuscules comptent.** Les fichiers doivent s'appeler exactement
> `ATLAS_INSTALLER.ELF` et `ATLASPS2.ELF`, en majuscules. Si Windows vous
> cache les extensions, vérifiez que vous n'avez pas
> `ATLASPS2.ELF.txt`.

---

## Étape 2 — Lancer l'installateur

1. **Éteignez la PS2.**
2. Mettez la Memory Card à installer dans le **port 1** (celui de gauche,
   quand vous êtes face à la console).
3. Branchez la clé USB sur un des deux ports USB à l'avant.
4. Allumez la PS2 et lancez votre environnement homebrew habituel
   (FreeMcBoot, uLaunchELF, etc.).
5. Dans cet environnement, ouvrez le périphérique USB. Suivant le
   programme il s'appelle `mass:` ou `USB`.
6. Placez le curseur sur `ATLAS_INSTALLER.ELF` et appuyez sur **×**
   (croix) pour le lancer.

L'écran devient noir une seconde, puis l'installateur s'affiche : fond
sombre, le titre **AtlasPS2 Installer** en haut, une liste au milieu.

> **Si la clé n'apparaît pas :** essayez l'autre port USB, puis une autre
> clé. La PS2 est difficile sur les clés USB, et c'est de loin le
> problème le plus fréquent. Ce n'est pas un signe que votre console a un
> problème.

---

## Étape 3 — Installer AtlasPS2

L'écran de l'installateur affiche d'abord un résumé, en haut :

```
Console      0160EC20030325
Carte        Memory Card (slot 1)
Espace       6 128 Ko libres
Source       mass:/ATLASPS2.ELF
État         Non installé
```

Vérifiez deux lignes avant de continuer :

* **Carte** doit dire *slot 1* (ou *slot 2* si vous avez mis la carte à
  droite). C'est la carte qui va être modifiée.
* **Source** ne doit pas dire *Aucune*. Si c'est le cas, l'installateur
  n'a pas trouvé `ATLASPS2.ELF` : retournez à l'étape 1, le fichier n'est
  pas au bon endroit ou pas au bon nom.

Ensuite :

1. Avec le **stick** ou la **croix directionnelle**, descendez sur
   **Installer AtlasPS2**.
2. Appuyez sur **×**.
3. Une demande de confirmation apparaît, avec le nom de la carte
   concernée. Relisez-la. Appuyez sur **×** pour confirmer, ou sur **○**
   pour annuler.

L'installation se déroule, cinq lignes qui se cochent une par une :

```
Installation d'AtlasPS2

Vérification              OK
Sauvegarde                OK
Copie du programme        OK
Configuration             OK
Vérification finale       OK

Installation terminée.
```

Cela prend une minute environ. **Ne retirez pas la carte et n'éteignez
pas la console pendant ce temps.** La ligne *Copie du programme* est la
plus longue : le programme fait environ 750 Ko, et une Memory Card écrit
lentement. Si l'écran semble figé quelques secondes, c'est normal.

Ce que fait chaque ligne, si vous voulez savoir :

| Ligne | Ce qui se passe |
|---|---|
| Vérification | La carte est lisible, il y a assez de place, le fichier source existe |
| Sauvegarde | Ce qui démarrait la console avant est copié dans `ATLAS/BACKUP` |
| Copie du programme | AtlasPS2 est écrit sous le nom temporaire `BOOT.NEW` |
| Configuration | Les dossiers `ATLAS/` et les réglages par défaut sont créés |
| Vérification finale | La copie est relue et comparée, puis seulement elle devient `BOOT.ELF` |

C'est la dernière ligne qui compte : **le fichier de démarrage qui
fonctionne n'est remplacé qu'après que la copie a été relue et vérifiée.**
Si quelque chose se passe mal avant, votre console démarre encore comme
avant.

4. Quand **Installation terminée** s'affiche, appuyez sur **×**.
5. Choisissez **Quitter** dans le menu, puis éteignez la console avec
   l'interrupteur à l'arrière.

---

## Étape 4 — Redémarrer

1. Laissez la Memory Card dans son port. Vous pouvez retirer la clé USB.
2. Rallumez la PS2 **sans appuyer sur aucun bouton**.
3. Après le logo Sony, AtlasPS2 s'affiche : fond sombre, **AtlasPS2** en
   haut à gauche, la version en haut à droite, et une liste :

```
Jeux
Applications
Fichiers
Périphériques
Vidéo
Thème
Réglages
Informations système
Alimentation
```

Au tout premier démarrage, un petit assistant apparaît avant : trois
questions (la langue, l'affichage, et s'il faut chercher vos
applications). Répondez avec **gauche/droite**, validez avec **×**.
C'est la seule fois où il apparaît.

**C'est fini.** Déplacez-vous avec la croix directionnelle, validez avec
**×**, revenez en arrière avec **○**.

### Mettre vos homebrews dedans

Copiez vos fichiers `.ELF` dans le dossier `ATLAS/APPS` de la Memory Card
ou de la clé USB. Ils apparaissent dans **Applications**. Vous pouvez
aussi les lancer depuis **Fichiers**, où qu'ils soient.

---

## En cas de problème

Les deux combinaisons ci-dessous se font **au moment où vous allumez la
console**. Maintenez les boutons appuyés dès que vous appuyez sur
l'interrupteur, et gardez-les jusqu'à ce qu'une image apparaisse.

### L'image est déformée, coupée, en noir et blanc, ou absente

Maintenez **R1** en allumant la console.

AtlasPS2 démarre en mode vidéo sûr : NTSC, 4:3, sans aucun décalage. Cela
marche sur pratiquement toutes les télévisions. Allez ensuite dans
**Vidéo** pour corriger le réglage fautif. Cet écran a un compte à rebours
de confirmation : si vous ne validez pas dans les quinze secondes, il
revient tout seul à l'ancien réglage — donc vous ne pouvez pas vous
enfermer dehors.

### AtlasPS2 ne démarre plus, ou plante, ou l'écran reste noir

Maintenez **L1 + R1** en allumant la console.

C'est le **mode Recovery**. Il se dessine sans thème, sans lire vos
réglages, avec un minimum de graphismes — précisément pour qu'un thème
cassé ou un fichier de configuration abîmé ne puisse pas vous bloquer
dehors. Il propose :

| Entrée | Ce qu'elle fait |
|---|---|
| Continuer | Démarrer normalement quand même |
| Réinitialiser les réglages | Efface `ATLAS.INI` et repart des valeurs d'usine |
| Désactiver le thème | Revient au thème intégré, qui ne peut pas manquer |
| Revenir à la version précédente | Remet la version d'AtlasPS2 d'avant la dernière mise à jour |
| Installer une mise à jour | Depuis `ATLAS_UPDATE` sur la clé USB |
| Changer de carte | Travailler sur la carte du port 2 |
| Retourner au navigateur | Le menu de la console |

**Revenir à la version précédente** est ce qu'il faut choisir si le
problème est apparu juste après une mise à jour.

### La console démarre comme avant, AtlasPS2 n'apparaît pas

Votre console ne démarre probablement pas depuis la Memory Card. Cela veut
dire que le bootstrap (FreeMcBoot ou équivalent) n'est pas installé sur
cette carte, ou qu'il est sur l'autre. AtlasPS2 s'installe *à côté* d'un
bootstrap, il n'en installe pas un — c'est une opération qui dépend du
modèle exact de la console, et se tromper peut rendre une carte
inutilisable, donc l'installateur ne devine pas.

### L'installateur affiche « Espace insuffisant »

Il refuse d'installer s'il reste moins de 1 Mo libre, pour ne pas remplir
la carte au point de casser vos sauvegardes. Supprimez quelques
sauvegardes de jeux dont vous n'avez plus besoin (avec le navigateur de la
console) et recommencez.

### L'installateur ne trouve pas la source

Retournez à l'étape 1. `ATLASPS2.ELF` doit être à la racine de la clé, en
majuscules, sans extension supplémentaire.

---

## Désinstaller AtlasPS2

Il y a deux opérations différentes et **il ne faut pas les confondre** :

* **Désinstaller** remet en place ce qui démarrait la console *avant*
  qu'AtlasPS2 soit installé. C'est ce que vous voulez.
* **Revenir à la version précédente** remet la version d'AtlasPS2 d'avant
  la dernière mise à jour. Ce n'est pas une désinstallation.

Pour désinstaller :

1. Copiez `ATLAS_INSTALLER.ELF` sur une clé USB, comme à l'étape 1.
2. Lancez l'installateur. Vous pouvez le lancer depuis AtlasPS2 lui-même
   (**Fichiers** → la clé USB → `ATLAS_INSTALLER.ELF` → **×**).
3. Vérifiez que la ligne **Carte** désigne bien la bonne carte.
4. Choisissez **Désinstaller AtlasPS2**, puis **×**, puis confirmez.
5. Éteignez, rallumez. La console redémarre comme avant.

L'installateur relit la sauvegarde faite à l'installation
(`ATLAS/BACKUP/BOOT.ELF`). Elle a été écrite **une seule fois**, à la
première installation, et n'est jamais réécrite — sinon, après une mise à
jour, « désinstaller » réinstallerait AtlasPS2.

Si cette sauvegarde n'existe pas (par exemple parce que la carte était
vide au départ), l'entrée **Désinstaller** est grisée et le dit. Dans ce
cas il n'y a rien à restaurer : supprimez `BOOT/BOOT.ELF` et le dossier
`ATLAS` avec uLaunchELF, ce qui rend la carte à son état d'avant.

Vos réglages AtlasPS2 restent dans `ATLAS/CONFIG` après une
désinstallation. Ils ne gênent rien, et si vous réinstallez plus tard vous
les retrouvez.

---

## Si rien de tout ça ne marche

Notez ce que vous voyez exactement — le modèle de console (l'étiquette
`SCPH-xxxxx` en dessous), la ligne **Console** affichée par
l'installateur, l'étape à laquelle ça bloque, et le message affiché s'il
y en a un. Ces quatre informations sont ce qu'il faut pour qu'on puisse
vous aider. Voir [COMPATIBILITY.md](COMPATIBILITY.md) pour le tableau des
configurations réellement testées.
