#include "emoji_picker.h"
#include "emojiwidgets.h"
#include "translator.h"
#include "placeholderlineedit.h"
#ifdef QT3_BUILD
#include <qptrlist.h>
#include <qtooltip.h>
#endif
#include <algorithm>

// ============ Emoji Data ============

static const EmojiData smileys_data[] = {
    {"😀", "grinning face"},
    {"😃", "grinning face with big eyes"},
    {"😄", "grinning face with smiling eyes"},
    {"😁", "beaming face with smiling eyes"},
    {"😆", "grinning squinting face"},
    {"😅", "grinning face with sweat"},
    {"🤣", "rolling on the floor laughing"},
    {"😂", "face with tears of joy"},
    {"🙂", "slightly smiling face"},
    {"🙃", "upside-down face"},
    {"😉", "winking face"},
    {"😊", "smiling face with smiling eyes"},
    {"😇", "smiling face with halo"},
    {"🥰", "smiling face with hearts"},
    {"😍", "smiling face with heart-eyes"},
    {"🤩", "star-struck"},
    {"😘", "face blowing a kiss"},
    {"😗", "kissing face"},
    {"😋", "face savoring food"},
    {"😛", "face with tongue"},
    {"😜", "winking face with tongue"},
    {"🤪", "zany face"},
    {"🤑", "money-mouth face"},
    {"🤗", "hugging face"},
    {"🤭", "face with hand over mouth"},
    {"🤫", "shushing face"},
    {"🤔", "thinking face"},
    {"🤐", "zipper-mouth face"},
    {"😐", "neutral face"},
    {"😑", "expressionless face"},
    {"😶", "face without mouth"},
    {"😏", "smirking face"},
    {"😒", "unamused face"},
    {"🙄", "face with rolling eyes"},
    {"😬", "grimacing face"},
    {"🤥", "lying face"},
    {"😌", "relieved face"},
    {"😔", "pensive face"},
    {"😪", "sleepy face"},
    {"🤤", "drooling face"},
    {"😴", "sleeping face"},
    {"🥹", "face holding back tears"},
    {"🫠", "melting face"},
    {"🫡", "saluting face"},
    {"😤", "face with steam from nose"},
    {"😠", "angry face"},
    {"😡", "enraged face"},
    {"🤬", "face with medical mask"},
    {"🥶", "cold face"},
    {"🥵", "hot face"},
    {"🥴", "woozy face"},
    {"😵", "dizzy face"},
    {"🤯", "exploding head"},
    {"🥳", "partying face"},
    {"💩", "pile of poo"},
    {"👻", "ghost"},
    {"💀", "skull"},
    {"☠️", "skull and crossbones"},
    {"🤡", "clown face"},
    {0, 0}
};

static const EmojiData people_data[] = {
    {"👋", "waving hand"},
    {"🤚", "raised back of hand"},
    {"✋", "raised hand"},
    {"🖐", "hand with fingers splayed"},
    {"✌", "victory hand"},
    {"🤞", "crossed fingers"},
    {"🤟", "love-you gesture"},
    {"🤘", "sign of the horns"},
    {"👌", "OK hand"},
    {"🤏", "pinching hand"},
    {"👈", "backhand index pointing left"},
    {"👉", "backhand index pointing right"},
    {"👆", "backhand index pointing up"},
    {"👇", "backhand index pointing down"},
    {"👍", "thumbs up"},
    {"👎", "thumbs down"},
    {"✊", "raised fist"},
    {"👊", "oncoming fist"},
    {"🤛", "left-facing fist"},
    {"🤜", "right-facing fist"},
    {"👏", "clapping hands"},
    {"🙌", "raising hands"},
    {"👐", "open hands"},
    {"🤲", "palms up together"},
    {"🤝", "handshake"},
    {"💅", "nail polish"},
    {"👂", "ear"},
    {"👃", "nose"},
    {"🧠", "brain"},
    {"🦶", "foot"},
    {"👀", "eyes"},
    {"👅", "tongue"},
    {"👄", "mouth"},
    {"👶", "baby"},
    {"🧒", "child"},
    {"👦", "boy"},
    {"👧", "girl"},
    {"🧑", "person"},
    {"👨", "man"},
    {"👩", "woman"},
    {"🧔", "bearded person"},
    {"👴", "old man"},
    {"👵", "old woman"},
    {"🫶", "heart hands"},
    {"🤌", "pinched fingers"},
    {"🫀", "anatomical heart"},
    {"🩻", "x-ray"},
    {"🦴", "bone"},
    {"🦷", "tooth"},
    {"👁", "eye"},
    {0, 0}
};

static const EmojiData nature_data[] = {
    {"🐶", "dog face"},
    {"🐱", "cat face"},
    {"🐭", "mouse face"},
    {"🐹", "hamster face"},
    {"🐰", "rabbit face"},
    {"🦊", "fox face"},
    {"🐻", "bear face"},
    {"🐼", "panda face"},
    {"🐨", "koala"},
    {"🦁", "lion face"},
    {"🐯", "tiger face"},
    {"🐮", "cow face"},
    {"🐷", "pig face"},
    {"🐸", "frog face"},
    {"🐵", "monkey face"},
    {"🐔", "chicken"},
    {"🐧", "penguin"},
    {"🐦", "bird"},
    {"🐤", "baby chick"},
    {"🦉", "owl"},
    {"🦇", "bat"},
    {"🐺", "wolf"},
    {"🐗", "boar"},
    {"🐴", "horse face"},
    {"🦄", "unicorn"},
    {"🐝", "honeybee"},
    {"🐛", "bug"},
    {"🦋", "butterfly"},
    {"🐌", "snail"},
    {"🐢", "turtle"},
    {"🐍", "snake"},
    {"🐲", "dragon face"},
    {"🐳", "spouting whale"},
    {"🐬", "dolphin"},
    {"🐟", "fish"},
    {"🐙", "octopus"},
    {"🌸", "cherry blossom"},
    {"🌺", "hibiscus"},
    {"🌻", "sunflower"},
    {"🌹", "rose"},
    {"🌷", "tulip"},
    {"🌲", "evergreen tree"},
    {"🌳", "deciduous tree"},
    {"🌴", "palm tree"},
    {"🍀", "four leaf clover"},
    {"🌵", "cactus"},
    {"🎄", "Christmas tree"},
    {"🌿", "herb"},
    {"☘", "shamrock"},
    {"🍃", "leaf fluttering in wind"},
    {"🍂", "fallen leaf"},
    {"🍁", "maple leaf"},
    {"🌾", "sheaf of rice"},
    {"🌊", "water wave"},
    {"💧", "droplet"},
    {0, 0}
};

static const EmojiData objects_data[] = {
    {"🍇", "grapes"},
    {"🍈", "melon"},
    {"🍉", "watermelon"},
    {"🍊", "tangerine"},
    {"🍋", "lemon"},
    {"🍌", "banana"},
    {"🍍", "pineapple"},
    {"🥭", "mango"},
    {"🍎", "red apple"},
    {"🍏", "green apple"},
    {"🍐", "pear"},
    {"🍑", "peach"},
    {"🍒", "cherries"},
    {"🍓", "strawberry"},
    {"🥝", "kiwi fruit"},
    {"🍅", "tomato"},
    {"🥥", "coconut"},
    {"🍞", "bread"},
    {"🧀", "cheese wedge"},
    {"🍕", "pizza"},
    {"🍔", "hamburger"},
    {"🍟", "french fries"},
    {"🌭", "hot dog"},
    {"🍿", "popcorn"},
    {"🥓", "bacon"},
    {"🍦", "soft ice cream"},
    {"🍧", "shaved ice"},
    {"🍨", "ice cream"},
    {"🍩", "doughnut"},
    {"🍪", "cookie"},
    {"🎂", "birthday cake"},
    {"🍫", "chocolate"},
    {"🍬", "candy"},
    {"🍭", "lollipop"},
    {"🍮", "custard"},
    {"☕", "hot beverage"},
    {"🍵", "teacup without handle"},
    {"🍺", "beer mug"},
    {"🍻", "clinking beer mugs"},
    {"🥂", "clinking glasses"},
    {"🥤", "cup with straw"},
    {"🥨", "pretzel"},
    {"🥖", "baguette bread"},
    {"🥯", "bagel"},
    {"🧇", "waffle"},
    {"🥞", "pancakes"},
    {"🧈", "butter"},
    {"🥜", "peanuts"},
    {"🌰", "chestnut"},
    {"🥛", "glass of milk"},
    {"🧃", "beverage box"},
    {"🧊", "ice"},
    {"🧉", "mate"},
    {"🥄", "spoon"},
    {"🍴", "fork and knife"},
    {"🥢", "chopsticks"},
    {"🍽", "fork and knife with plate"},
    {0, 0}
};

static const EmojiData symbols_data[] = {
    {"❤", "red heart"},
    {"🧡", "orange heart"},
    {"💛", "yellow heart"},
    {"💚", "green heart"},
    {"💙", "blue heart"},
    {"💜", "purple heart"},
    {"🖤", "black heart"},
    {"🤍", "white heart"},
    {"💔", "broken heart"},
    {"❣", "heart exclamation"},
    {"💕", "two hearts"},
    {"💞", "revolving hearts"},
    {"💗", "growing heart"},
    {"💖", "sparkling heart"},
    {"💘", "heart with arrow"},
    {"💝", "heart with ribbon"},
    {"⭐", "star"},
    {"🌟", "glowing star"},
    {"✨", "sparkles"},
    {"💫", "dizzy"},
    {"💥", "collision"},
    {"💯", "hundred points"},
    {"🔥", "fire"},
    {"💦", "sweat droplets"},
    {"💨", "dashing away"},
    {"💣", "bomb"},
    {"💬", "speech balloon"},
    {"💤", "zzz"},
    {"🚀", "rocket"},
    {"🛸", "flying saucer"},
    {"🌍", "globe showing Europe-Africa"},
    {"🌏", "globe showing Asia-Australia"},
    {"🌎", "globe showing Americas"},
    {"🎉", "party popper"},
    {"🎊", "confetti ball"},
    {"🏁", "chequered flag"},
    {"🚩", "triangular flag"},
    {"🎈", "balloon"},
    {"🎁", "wrapped gift"},
    {"🏆", "trophy"},
    {"💎", "gem stone"},
    {"💊", "pill"},
    {"🩸", "drop of blood"},
    {"🩹", "adhesive bandage"},
    {"🩺", "stethoscope"},
    {"💉", "syringe"},
    {"🔪", "kitchen knife"},
    {"🗡", "dagger"},
    {"🔫", "pistol"},
    {"🛡", "shield"},
    {"🔧", "wrench"},
    {"🔨", "hammer"},
    {"🪛", "screwdriver"},
    {"🔩", "nut and bolt"},
    {"⚙️", "gear"},
    {"🧰", "toolbox"},
    {"🧲", "magnet"},
    {"🧪", "test tube"},
    {"🧫", "petri dish"},
    {"🔬", "microscope"},
    {"🔭", "telescope"},
    {"📡", "satellite antenna"},
    {"💡", "light bulb"},
    {"🔦", "flashlight"},
    {"🕯", "candle"},
    {"🪫", "low battery"},
    {"🔋", "battery"},
    {"📀", "DVD"},
    {"💿", "CD"},
    {"💽", "minidisc"},
    {"💾", "floppy disk"},
    {"💻", "laptop"},
    {"🖥", "desktop computer"},
    {"🖨", "printer"},
    {"⌨", "keyboard"},
    {"🖱", "computer mouse"},
    {"🖲", "trackball"},
    {"📷", "camera"},
    {"📸", "camera with flash"},
    {"📹", "video camera"},
    {"📼", "videocassette"},
    {"🔍", "magnifying glass tilted left"},
    {"🔎", "magnifying glass tilted right"},
    {"🔮", "crystal ball"},
    {"🧿", "nazar amulet"},
    {"🎯", "direct hit"},
    {"♟", "chess pawn"},
    {"🎰", "slot machine"},
    {0, 0}
};

static const EmojiData activities_data[] = {
    {"⚽", "soccer ball"},
    {"🏀", "basketball"},
    {"🏈", "american football"},
    {"⚾", "baseball"},
    {"🎾", "tennis"},
    {"🏐", "volleyball"},
    {"🏓", "ping pong"},
    {"🏸", "badminton"},
    {"🥊", "boxing glove"},
    {"🥋", "martial arts uniform"},
    {"🎱", "pool 8 ball"},
    {"🛹", "skateboard"},
    {"⛸", "ice skate"},
    {"🥌", "curling stone"},
    {"🎿", "skis"},
    {"⛷", "skier"},
    {"🏂", "snowboarder"},
    {"🎮", "video game"},
    {"🕹", "joystick"},
    {"🎲", "game die"},
    {"🧩", "puzzle piece"},
    {"🎭", "performing arts"},
    {"🎨", "artist palette"},
    {"🎬", "clapper board"},
    {"🎤", "microphone"},
    {"🎧", "headphone"},
    {"🎵", "musical note"},
    {"🎶", "musical notes"},
    {"🎼", "musical score"},
    {"🎹", "musical keyboard"},
    {"🥁", "drum"},
    {"🎺", "trumpet"},
    {"🎸", "guitar"},
    {"🪘", "long drum"},
    {"🎪", "circus tent"},
    {"🎟", "admission tickets"},
    {"🎫", "ticket"},
    {"🎖", "military medal"},
    {"🏅", "sports medal"},
    {"🥇", "1st place medal"},
    {"🥈", "2nd place medal"},
    {"🥉", "3rd place medal"},
    {"🏆", "trophy"},
    {"🎳", "bowling"},
    {0, 0}
};

static const EmojiData travel_data[] = {
    {"✈️", "airplane"},
    {"🚁", "helicopter"},
    {"🚂", "locomotive"},
    {"🚃", "railway car"},
    {"🚇", "metro"},
    {"🚉", "station"},
    {"🚌", "bus"},
    {"🚎", "trolleybus"},
    {"🚐", "minibus"},
    {"🚕", "taxi"},
    {"🚗", "automobile"},
    {"🚙", "sport utility vehicle"},
    {"🚚", "delivery truck"},
    {"🚛", "articulated lorry"},
    {"🚲", "bicycle"},
    {"🛵", "motor scooter"},
    {"🛴", "kick scooter"},
    {"🚶", "person walking"},
    {"🏃", "person running"},
    {"🏠", "house"},
    {"🏡", "house with garden"},
    {"🏢", "office building"},
    {"🏣", "Japanese post office"},
    {"🏤", "post office"},
    {"🏥", "hospital"},
    {"🏦", "bank"},
    {"🏨", "hotel"},
    {"🏩", "love hotel"},
    {"🏪", "convenience store"},
    {"🏫", "school"},
    {"🏬", "department store"},
    {"🏯", "Japanese castle"},
    {"🏰", "castle"},
    {"🏭", "factory"},
    {"🏗", "building construction"},
    {"🗽", "Statue of Liberty"},
    {"🗼", "Tokyo tower"},
    {"🗿", "moai"},
    {"🌋", "volcano"},
    {"🏔", "snow-capped mountain"},
    {"🏕", "campsite"},
    {"🏖", "beach with umbrella"},
    {"🏝", "desert island"},
    {"🌄", "sunrise over mountains"},
    {"🌅", "sunrise"},
    {"🗾", "map of Japan"},
    {0, 0}
};

static const EmojiData clothing_data[] = {
    {"👗", "dress"},
    {"👘", "kimono"},
    {"🥻", "sari"},
    {"👚", "woman's clothes"},
    {"👕", "t-shirt"},
    {"👔", "neck tie"},
    {"👖", "jeans"},
    {"🧥", "coat"},
    {"🥼", "lab coat"},
    {"🦺", "safety vest"},
    {"👟", "running shoe"},
    {"👞", "man's shoe"},
    {"👡", "woman's sandal"},
    {"👠", "high-heeled shoe"},
    {"👢", "woman's boot"},
    {"👒", "woman's hat"},
    {"🎩", "top hat"},
    {"🧢", "billed cap"},
    {"⛑", "rescue worker's helmet"},
    {"🎓", "graduation cap"},
    {"🧣", "scarf"},
    {"🧤", "gloves"},
    {"🧦", "socks"},
    {"👙", "bikini"},
    {0, 0}
};

static const EmojiCategory categories[] = {
    {"emoji.smileys", "😀", smileys_data},
    {"emoji.people", "👋", people_data},
    {"emoji.nature", "🐶", nature_data},
    {"emoji.objects", "🍎", objects_data},
    {"emoji.symbols", "❤", symbols_data},
    {"emoji.activities", "⚽", activities_data},
    {"emoji.travel", "✈️", travel_data},
    {"emoji.clothing", "👗", clothing_data},
    {0, 0, 0}
};

// ============ EmojiPicker ============

EmojiPicker::EmojiPicker(QWidget* parent)
#ifndef QT3_BUILD
    : QWidget(parent, Qt::Popup)
#else
    : QWidget(parent, 0, WType_Popup | WStyle_StaysOnTop)
#endif
    , m_activeCategory(0)
    , m_emojiCellSize(32)
    , m_recentSection(0)
{
    setFixedWidth(m_emojiCellSize * GRID_COLS + 16 + 200);
    setFixedHeight(400);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setSpacing(2);
    qSetMargins(layout, 4, 4, 4, 4);

    // Search bar
    m_searchBar = new PlaceholderLineEdit(_("emoji.search_placeholder"), this);
    layout->addWidget(m_searchBar);
    connect(m_searchBar, SIGNAL(textChanged(const QString&)),
            this, SLOT(onSearchChanged(const QString&)));

    // Recent label
    m_recentLabel = new QLabel(_("emoji.recently_used"), this);
    QFont recentFont = m_recentLabel->font();
    recentFont.setPointSize(recentFont.pointSize() - 2);
    m_recentLabel->setFont(recentFont);
#ifdef QT3_BUILD
    m_recentLabel->hide();
#else
    m_recentLabel->setVisible(false);
#endif
    layout->addWidget(m_recentLabel);

    // Tab bar
    m_tabBar = new QWidget(this);
    QHBoxLayout* tabLayout = new QHBoxLayout(m_tabBar);
    tabLayout->setSpacing(2);
    qSetMargins(tabLayout, 0, 0, 0, 0);

    static const char* tabText[] = {"Smileys","People","Nature","Food","Symbols","Activities","Travel","Clothing",0};
    for (int i = 0; categories[i].key; i++) {
        QPushButton* btn = new QPushButton(tr(tabText[i]), m_tabBar);
        btn->setFixedHeight(m_emojiCellSize);
        btn->setFlat(true);
        connect(btn, SIGNAL(clicked()), this, SLOT(onTabClicked()));
        tabLayout->addWidget(btn);
        m_tabButtons.push_back(btn);
    }
    layout->addWidget(m_tabBar);

    // Grid area: contains optional recent section + page stack
    m_gridArea = new QWidget(this);
    layout->addWidget(m_gridArea);
    QVBoxLayout* areaLayout = new QVBoxLayout(m_gridArea);
    areaLayout->setSpacing(0);
    qSetMargins(areaLayout, 0, 0, 0, 0);

    // Page stack: one page per category + search results page
    m_pageStack = new StackedWidget(m_gridArea);
    areaLayout->addWidget(m_pageStack, 1);

    buildCategoryPages();

    // Search results page (initially empty)
    m_searchPage = new QWidget(m_pageStack);
    m_pageStack->addWidget(m_searchPage);

    // Show first category by default
    if (!m_categoryPages.empty())
        qStackSetCurrent(m_pageStack, m_categoryPages[0]);

    // Rebuild recent section (initially empty → adds nothing)
    rebuildRecentSection();
}

void EmojiPicker::showAt(const QPoint& pos) {
    layout()->activate();
    move(pos);
    show();
}

void EmojiPicker::buildCategoryPages() {
    for (int ci = 0; categories[ci].key; ci++) {
        QWidget* page = new QWidget(m_pageStack);
#ifdef QT3_BUILD
        QGridLayout* grid = new QGridLayout(page, 1, 1, 1);
#else
        QGridLayout* grid = new QGridLayout(page);
        grid->setSpacing(1);
#endif
        for (int ei = 0; categories[ci].items[ei].emoji; ei++) {
            const EmojiData& d = categories[ci].items[ei];
            int row = ei / GRID_COLS;
            int col = ei % GRID_COLS;
            EmojiPushButton* btn = new EmojiPushButton(qFromUtf8(d.emoji), page);
            btn->setFixedSize(m_emojiCellSize, m_emojiCellSize);
            btn->setFlat(true);
            qSetToolTip(btn, qFromUtf8(d.name));
            connect(btn, SIGNAL(clicked()), this, SLOT(onEmojiButtonClicked()));
            grid->addWidget(btn, row, col);
        }
        m_pageStack->addWidget(page);
        m_categoryPages.push_back(page);
    }
}

void EmojiPicker::rebuildRecentSection() {
    if (m_recentSection) {
#ifdef QT3_BUILD
        m_gridArea->layout()->remove(m_recentSection);
#else
        m_gridArea->layout()->removeWidget(m_recentSection);
#endif
        delete m_recentSection;
        m_recentSection = 0;
    }

    bool hasRecent = !m_recentEmojis.isEmpty();
#ifdef QT3_BUILD
    if (hasRecent) {
        m_recentLabel->show();
    } else {
        m_recentLabel->hide();
    }
#else
    m_recentLabel->setVisible(hasRecent);
#endif

    if (!hasRecent) { return; }

    m_recentSection = new QWidget(m_gridArea);
#ifdef QT3_BUILD
    QGridLayout* grid = new QGridLayout(m_recentSection, 1, 1, 1);
#else
    QGridLayout* grid = new QGridLayout(m_recentSection);
    grid->setSpacing(1);
#endif
    int n = (int)m_recentEmojis.size() < MAX_RECENT ? (int)m_recentEmojis.size() : MAX_RECENT;
    for (int i = 0; i < n; i++) {
        int row = i / GRID_COLS;
        int col = i % GRID_COLS;
        EmojiPushButton* btn = new EmojiPushButton(m_recentEmojis[i], m_recentSection);
        btn->setFixedSize(m_emojiCellSize, m_emojiCellSize);
        btn->setFlat(true);
        connect(btn, SIGNAL(clicked()), this, SLOT(onEmojiButtonClicked()));
        grid->addWidget(btn, row, col);
    }

    // Insert at top of area layout (before the stack)
    QBoxLayout* areaLayout = (QBoxLayout*)m_gridArea->layout();
    areaLayout->insertWidget(0, m_recentSection);
}

void EmojiPicker::rebuildSearchPage() {
    // Clear old child widgets
#ifdef QT3_BUILD
    {
        QPtrList<QObject> kids;
        if (m_searchPage->children())
            kids = *(QPtrList<QObject>*)m_searchPage->children();
        for (uint i = 0; i < kids.count(); i++) {
            QObject* obj = kids.at(i);
            if (obj->isWidgetType())
                delete (QWidget*)obj;
        }
    }
#else
    {
        QList<QObject*> kids = m_searchPage->children();
        for (int i = 0; i < kids.size(); i++) {
            QObject* obj = kids[i];
            if (obj->isWidgetType())
                delete static_cast<QWidget*>(obj);
        }
    }
#endif
    // Delete old layout to remove stale items (Qt3 issue)
    QLayout* oldLayout = m_searchPage->layout();
    delete oldLayout;

    if (m_searchQuery.isEmpty()) return;

    // Collect matching items across all categories
    struct Item { const char* emoji; const char* name; };
    std::vector<Item> items;
    QString q = m_searchQuery;
    for (int ci = 0; categories[ci].key; ci++) {
        for (int ei = 0; categories[ci].items[ei].emoji; ei++) {
            const EmojiData& d = categories[ci].items[ei];
#ifdef QT3_BUILD
            if (QString(d.name).find(q, 0, false) >= 0)
#else
            if (QString(d.name).contains(q, Qt::CaseInsensitive))
#endif
            {
                Item it = {d.emoji, d.name};
                items.push_back(it);
            }
        }
    }
    if (items.empty()) return;

    // Build fresh grid
    QVBoxLayout* innerLayout = new QVBoxLayout(m_searchPage);
    innerLayout->setSpacing(1);
#ifdef QT3_BUILD
    qSetLayoutMargin(innerLayout, 0);
#else
    innerLayout->setContentsMargins(0, 0, 0, 0);
#endif

    QWidget* emojiW = new QWidget(m_searchPage);
#ifdef QT3_BUILD
    QGridLayout* pageLayout = new QGridLayout(emojiW, 1, 1, 1);
#else
    QGridLayout* pageLayout = new QGridLayout(emojiW);
    pageLayout->setSpacing(1);
#endif
    int n = (int)items.size();
    for (int i = 0; i < n; i++) {
        int row = i / GRID_COLS;
        int col = i % GRID_COLS;
        EmojiPushButton* btn = new EmojiPushButton(qFromUtf8(items[i].emoji), emojiW);
        btn->setFixedSize(m_emojiCellSize, m_emojiCellSize);
        btn->setFlat(true);
        qSetToolTip(btn, qFromUtf8(items[i].name));
        connect(btn, SIGNAL(clicked()), this, SLOT(onEmojiButtonClicked()));
        pageLayout->addWidget(btn, row, col);
    }
    innerLayout->addWidget(emojiW);
    innerLayout->activate();
}

void EmojiPicker::onSearchChanged(const QString& query) {
    m_searchQuery = query;

    if (query.isEmpty()) {
        // No search → switch back to category page, show tab bar
        qStackSetCurrent(m_pageStack, m_categoryPages[m_activeCategory]);
#ifdef QT3_BUILD
        m_tabBar->show();
        if (!m_recentEmojis.isEmpty())
            m_recentLabel->show();
        else
            m_recentLabel->hide();
#else
        m_tabBar->setVisible(true);
        m_recentLabel->setVisible(!m_recentEmojis.isEmpty());
#endif
        return;
    }

    // Searching → show search page, hide tab bar and recent label
    rebuildSearchPage();
    qStackSetCurrent(m_pageStack, m_searchPage);
#ifdef QT3_BUILD
    m_tabBar->hide();
    m_recentLabel->hide();
#else
    m_tabBar->setVisible(false);
    m_recentLabel->setVisible(false);
#endif
}

void EmojiPicker::onTabClicked() {
    QPushButton* btn = (QPushButton*)sender();
    for (size_t i = 0; i < m_tabButtons.size(); i++) {
        if (m_tabButtons[i] == btn) {
            m_activeCategory = (int)i;
            break;
        }
    }
    m_searchBar->clear();
    m_searchQuery = QString();

    qStackSetCurrent(m_pageStack, m_categoryPages[m_activeCategory]);

#ifdef QT3_BUILD
    m_tabBar->show();
    if (!m_recentEmojis.isEmpty())
        m_recentLabel->show();
    else
        m_recentLabel->hide();
#else
    m_tabBar->setVisible(true);
    m_recentLabel->setVisible(!m_recentEmojis.isEmpty());
#endif
}

void EmojiPicker::onEmojiButtonClicked() {
    EmojiPushButton* btn = (EmojiPushButton*)sender();
    QString emoji = btn->text();
    addToRecent(emoji);
    rebuildRecentSection();
    emit emojiSelected(emoji);
    hide();
}

void EmojiPicker::addToRecent(const QString& emoji) {
#ifdef QT3_BUILD
    m_recentEmojis.remove(emoji);
#else
    m_recentEmojis.removeAll(emoji);
#endif
    m_recentEmojis.prepend(emoji);
    while (m_recentEmojis.size() > MAX_RECENT) {
        m_recentEmojis.pop_back();
    }
}
