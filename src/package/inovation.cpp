#include "inovation.h"
#include "serverplayer.h"
#include "room.h"
#include "skill.h"
#include "maneuvering.h"
#include "clientplayer.h"
#include "engine.h"
#include "client.h"
#include "exppattern.h"
#include "roomthread.h"
#include "wrapped-card.h"
#include "json.h"
#include "settings.h"

//standard
class Yingzi : public DrawCardsSkill
{
public:
    Yingzi() : DrawCardsSkill("yingzi")
    {
        frequency = Compulsory;
    }

    int getDrawNum(ServerPlayer *zhouyu, int n) const
    {
        Room *room = zhouyu->getRoom();

        int index = qrand() % 2 + 1;
        if (!zhouyu->hasInnateSkill(this)) {
            if (zhouyu->hasSkill("hunzi"))
                index = 5;
            else if (zhouyu->hasSkill("mouduan"))
                index += 2;
        }
        room->broadcastSkillInvoke(objectName(), index);
        room->sendCompulsoryTriggerLog(zhouyu, objectName());

        return n + 1;
    }
};

class YingziMaxCards : public MaxCardsSkill
{
public:
    YingziMaxCards() : MaxCardsSkill("#yingzi")
    {
    }

    int getFixed(const Player *target) const
    {
        if (target->hasSkill("yingzi"))
            return target->getMaxHp();
        else
            return -1;
    }
};

class Fankui : public MasochismSkill
{
public:
    Fankui() : MasochismSkill("fankui")
    {
    }

    void onDamaged(ServerPlayer *simayi, const DamageStruct &damage) const
    {
        ServerPlayer *from = damage.from;
        Room *room = simayi->getRoom();
        for (int i = 0; i < damage.damage; i++) {
            QVariant data = QVariant::fromValue(from);
            if (from && !from->isNude() && room->askForSkillInvoke(simayi, "fankui", data)) {
                room->broadcastSkillInvoke(objectName());
                int card_id = room->askForCardChosen(simayi, from, "he", "fankui");
                CardMoveReason reason(CardMoveReason::S_REASON_EXTRACTION, simayi->objectName());
                room->obtainCard(simayi, Sanguosha->getCard(card_id),
                    reason, room->getCardPlace(card_id) != Player::PlaceHand);
            }
            else {
                break;
            }
        }
    }
};

class Guicai : public RetrialSkill
{
public:
    Guicai() : RetrialSkill("guicai")
    {

    }

    const Card *onRetrial(ServerPlayer *player, JudgeStruct *judge) const
    {
        if (player->isNude())
            return NULL;

        QStringList prompt_list;
        prompt_list << "@guicai-card" << judge->who->objectName()
            << objectName() << judge->reason << QString::number(judge->card->getEffectiveId());
        QString prompt = prompt_list.join(":");
        bool forced = false;
        if (player->getMark("JilveEvent") == int(AskForRetrial))
            forced = true;

        Room *room = player->getRoom();

        const Card *card = room->askForCard(player, forced ? "..!" : "..", prompt, QVariant::fromValue(judge), Card::MethodResponse, judge->who, true);
        if (forced && card == NULL) {
            QList<const Card *> c = player->getCards("he");
            card = c.at(qrand() % c.length());
        }

        if (card) {
            if (player->hasInnateSkill("guicai") || !player->hasSkill("jilve"))
                room->broadcastSkillInvoke(objectName());
            else
                room->broadcastSkillInvoke("jilve", 1);
        }

        return card;
    }
};

class Longdan : public OneCardViewAsSkill
{
public:
    Longdan() : OneCardViewAsSkill("longdan")
    {
        response_or_use = true;
    }

    bool viewFilter(const Card *to_select) const
    {
        const Card *card = to_select;

        switch (Sanguosha->currentRoomState()->getCurrentCardUseReason()) {
        case CardUseStruct::CARD_USE_REASON_PLAY: {
            return card->isKindOf("Jink");
        }
        case CardUseStruct::CARD_USE_REASON_RESPONSE:
        case CardUseStruct::CARD_USE_REASON_RESPONSE_USE: {
            QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
            if (pattern == "slash")
                return card->isKindOf("Jink");
            else if (pattern == "jink")
                return card->isKindOf("Slash");
        }
        default:
            return false;
        }
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return Slash::IsAvailable(player);
    }

    bool isEnabledAtResponse(const Player *, const QString &pattern) const
    {
        return pattern == "jink" || pattern == "slash";
    }

    const Card *viewAs(const Card *originalCard) const
    {
        if (originalCard->isKindOf("Slash")) {
            Jink *jink = new Jink(originalCard->getSuit(), originalCard->getNumber());
            jink->addSubcard(originalCard);
            jink->setSkillName(objectName());
            return jink;
        }
        else if (originalCard->isKindOf("Jink")) {
            Slash *slash = new Slash(originalCard->getSuit(), originalCard->getNumber());
            slash->addSubcard(originalCard);
            slash->setSkillName(objectName());
            return slash;
        }
        else
            return NULL;
    }

    int getEffectIndex(const ServerPlayer *player, const Card *) const
    {
        int index = qrand() % 2 + 1;
        if (Player::isNostalGeneral(player, "zhaoyun"))
            index += 2;
        return index;
    }
};

class Paoxiao : public TargetModSkill
{
public:
    Paoxiao() : TargetModSkill("paoxiao")
    {
        frequency = NotCompulsory;
    }

    int getResidueNum(const Player *from, const Card *) const
    {
        if (from->hasSkill(this))
            return 1000;
        else
            return 0;
    }
};

class Wansha : public TriggerSkill
{
public:
    Wansha() : TriggerSkill("wansha")
    {
        // just to broadcast audio effects and to send log messages
        // main part in the AskForPeaches trigger of Game Rule
        events << AskForPeaches;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    int getPriority(TriggerEvent) const
    {
        return 7;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (player == room->getAllPlayers().first()) {
            DyingStruct dying = data.value<DyingStruct>();
            ServerPlayer *jiaxu = room->getCurrent();
            if (!jiaxu || !TriggerSkill::triggerable(jiaxu) || jiaxu->getPhase() == Player::NotActive)
                return false;
            if (jiaxu->hasInnateSkill("wansha") || !jiaxu->hasSkill("jilve"))
                room->broadcastSkillInvoke(objectName());
            else
                room->broadcastSkillInvoke("jilve", 3);

            room->notifySkillInvoked(jiaxu, objectName());

            LogMessage log;
            log.from = jiaxu;
            log.arg = objectName();
            if (jiaxu != dying.who) {
                log.type = "#WanshaTwo";
                log.to << dying.who;
            }
            else {
                log.type = "#WanshaOne";
            }
            room->sendLog(log);
        }
        return false;
    }
};

class Wumou : public TriggerSkill
{
public:
    Wumou() : TriggerSkill("wumou")
    {
        frequency = Compulsory;
        events << CardUsed;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isNDTrick()) {
            room->broadcastSkillInvoke(objectName());
            room->sendCompulsoryTriggerLog(player, objectName());

            int num = player->getMark("@wrath");
            if (num >= 1 && room->askForChoice(player, objectName(), "discard+losehp") == "discard") {
                player->loseMark("@wrath");
            }
            else
                room->loseHp(player);
        }

        return false;
    }
};

class Benghuai : public PhaseChangeSkill
{
public:
    Benghuai() : PhaseChangeSkill("benghuai")
    {
        frequency = Compulsory;
    }

    bool onPhaseChange(ServerPlayer *dongzhuo) const
    {
        bool trigger_this = false;
        Room *room = dongzhuo->getRoom();

        if (dongzhuo->getPhase() == Player::Finish) {
            QList<ServerPlayer *> players = room->getOtherPlayers(dongzhuo);
            foreach(ServerPlayer *player, players) {
                if (dongzhuo->getHp() > player->getHp()) {
                    trigger_this = true;
                    break;
                }
            }
        }

        if (trigger_this) {
            room->sendCompulsoryTriggerLog(dongzhuo, objectName());

            QString result = room->askForChoice(dongzhuo, "benghuai", "hp+maxhp");
            int index = (dongzhuo->isFemale()) ? 2 : 1;

            if (!dongzhuo->hasInnateSkill(this) && dongzhuo->getMark("juyi") > 0)
                index = 3;

            if (!dongzhuo->hasInnateSkill(this) && dongzhuo->getMark("baoling") > 0)
                index = result == "hp" ? 4 : 5;

            room->broadcastSkillInvoke(objectName(), index);
            if (result == "hp")
                room->loseHp(dongzhuo);
            else
                room->loseMaxHp(dongzhuo);
        }

        return false;
    }
};

TiaoxinCard::TiaoxinCard()
{
}

bool TiaoxinCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.isEmpty() && to_select->inMyAttackRange(Self);
}

void TiaoxinCard::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.from->getRoom();
    bool use_slash = false;
    if (effect.to->canSlash(effect.from, NULL, false))
        use_slash = room->askForUseSlashTo(effect.to, effect.from, "@tiaoxin-slash:" + effect.from->objectName());
    if (!use_slash && effect.from->canDiscard(effect.to, "he"))
        room->throwCard(room->askForCardChosen(effect.from, effect.to, "he", "tiaoxin", false, Card::MethodDiscard), effect.to, effect.from);
}

class Tiaoxin : public ZeroCardViewAsSkill
{
public:
    Tiaoxin() : ZeroCardViewAsSkill("tiaoxin")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("TiaoxinCard");
    }

    const Card *viewAs() const
    {
        return new TiaoxinCard;
    }

    int getEffectIndex(const ServerPlayer *player, const Card *) const
    {
        int index = qrand() % 2 + 1;
        if (!player->hasInnateSkill(this) && player->hasSkill("baobian"))
            index += 3;
        else if (!player->hasInnateSkill(this) && player->getMark("fengliang") > 0)
            index += 5;
        else if (player->hasArmorEffect("eight_diagram"))
            index = 3;
        return index;
    }
};




QString BasicCard::getType() const
{
    return "basic";
}

Card::CardType BasicCard::getTypeId() const
{
    return TypeBasic;
}

TrickCard::TrickCard(Suit suit, int number)
    : Card(suit, number), cancelable(true)
{
    handling_method = Card::MethodUse;
}

void TrickCard::setCancelable(bool cancelable)
{
    this->cancelable = cancelable;
}

QString TrickCard::getType() const
{
    return "trick";
}

Card::CardType TrickCard::getTypeId() const
{
    return TypeTrick;
}

bool TrickCard::isCancelable(const CardEffectStruct &effect) const
{
    Q_UNUSED(effect);
    return cancelable;
}

QString EquipCard::getType() const
{
    return "equip";
}

Card::CardType EquipCard::getTypeId() const
{
    return TypeEquip;
}

bool EquipCard::isAvailable(const Player *player) const
{
    return !player->isProhibited(player, this) && Card::isAvailable(player);
}

void EquipCard::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;

    ServerPlayer *player = use.from;
    if (use.to.isEmpty())
        use.to << player;

    QVariant data = QVariant::fromValue(use);
    RoomThread *thread = room->getThread();
    thread->trigger(PreCardUsed, room, player, data);
    thread->trigger(CardUsed, room, player, data);
    thread->trigger(CardFinished, room, player, data);
}

void EquipCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    if (targets.isEmpty()) {
        CardMoveReason reason(CardMoveReason::S_REASON_USE, source->objectName(), QString(), this->getSkillName(), QString());
        room->moveCardTo(this, NULL, Player::DiscardPile, reason, true);
    }
    int equipped_id = Card::S_UNKNOWN_CARD_ID;
    ServerPlayer *target = targets.first();
    if (target->getEquip(location()))
        equipped_id = target->getEquip(location())->getEffectiveId();

    QList<CardsMoveStruct> exchangeMove;
    CardsMoveStruct move1(getEffectiveId(), target, Player::PlaceEquip,
        CardMoveReason(CardMoveReason::S_REASON_USE, target->objectName()));
    exchangeMove.push_back(move1);
    if (equipped_id != Card::S_UNKNOWN_CARD_ID) {
        CardsMoveStruct move2(equipped_id, NULL, Player::DiscardPile,
            CardMoveReason(CardMoveReason::S_REASON_CHANGE_EQUIP, target->objectName()));
        exchangeMove.push_back(move2);
    }
    LogMessage log;
    log.from = target;
    log.type = "$Install";
    log.card_str = QString::number(getEffectiveId());
    room->sendLog(log);

    room->moveCardsAtomic(exchangeMove, true);
}

static bool isEquipSkillViewAsSkill(const Skill *s)
{
    if (s == NULL)
        return false;

    if (s->inherits("ViewAsSkill"))
        return true;

    if (s->inherits("TriggerSkill")) {
        const TriggerSkill *ts = qobject_cast<const TriggerSkill *>(s);
        if (ts == NULL)
            return false;

        if (ts->getViewAsSkill() != NULL)
            return true;
    }

    return false;
}

void EquipCard::onInstall(ServerPlayer *player) const
{
    const Skill *skill = Sanguosha->getSkill(this);

    if (skill != NULL) {
        Room *room = player->getRoom();
        if (skill->inherits("TriggerSkill")) {
            const TriggerSkill *trigger_skill = qobject_cast<const TriggerSkill *>(skill);
            room->getThread()->addTriggerSkill(trigger_skill);
        }

        if (isEquipSkillViewAsSkill(skill))
            room->attachSkillToPlayer(player, objectName());
    }
}

void EquipCard::onUninstall(ServerPlayer *player) const
{
    const Skill *skill = Sanguosha->getSkill(this);
    if (isEquipSkillViewAsSkill(skill))
        player->getRoom()->detachSkillFromPlayer(player, objectName(), true);
}

QString GlobalEffect::getSubtype() const
{
    return "global_effect";
}

void GlobalEffect::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *source = card_use.from;
    QList<ServerPlayer *> targets, all_players = room->getAllPlayers();
    foreach(ServerPlayer *player, all_players) {
        const ProhibitSkill *skill = room->isProhibited(source, player, this);
        if (skill) {
            if (skill->isVisible()) {
                LogMessage log;
                log.type = "#SkillAvoid";
                log.from = player;
                log.arg = skill->objectName();
                log.arg2 = objectName();
                room->sendLog(log);

                room->broadcastSkillInvoke(skill->objectName());
            }
        }
        else
            targets << player;
    }

    CardUseStruct use = card_use;
    use.to = targets;
    TrickCard::onUse(room, use);
}

bool GlobalEffect::isAvailable(const Player *player) const
{
    bool canUse = false;
    QList<const Player *> players = player->getAliveSiblings();
    players << player;
    foreach(const Player *p, players) {
        if (player->isProhibited(p, this))
            continue;

        canUse = true;
        break;
    }

    return canUse && TrickCard::isAvailable(player);
}

QString AOE::getSubtype() const
{
    return "aoe";
}

bool AOE::isAvailable(const Player *player) const
{
    bool canUse = false;
    QList<const Player *> players = player->getAliveSiblings();
    foreach(const Player *p, players) {
        if (player->isProhibited(p, this))
            continue;

        canUse = true;
        break;
    }

    return canUse && TrickCard::isAvailable(player);
}

void AOE::onUse(Room *room, const CardUseStruct &card_use) const
{
    ServerPlayer *source = card_use.from;
    QList<ServerPlayer *> targets, other_players = room->getOtherPlayers(source);
    foreach(ServerPlayer *player, other_players) {
        const ProhibitSkill *skill = room->isProhibited(source, player, this);
        if (skill) {
            if (skill->isVisible()) {
                LogMessage log;
                log.type = "#SkillAvoid";
                log.from = player;
                log.arg = skill->objectName();
                log.arg2 = objectName();
                room->sendLog(log);

                room->broadcastSkillInvoke(skill->objectName());
            }
        }
        else
            targets << player;
    }

    CardUseStruct use = card_use;
    use.to = targets;
    TrickCard::onUse(room, use);
}

QString SingleTargetTrick::getSubtype() const
{
    return "single_target_trick";
}

bool SingleTargetTrick::targetFilter(const QList<const Player *> &, const Player *, const Player *) const
{
    return true;
}

DelayedTrick::DelayedTrick(Suit suit, int number, bool movable)
    : TrickCard(suit, number), movable(movable)
{
    judge.negative = true;
}

void DelayedTrick::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;
    WrappedCard *wrapped = Sanguosha->getWrappedCard(this->getEffectiveId());
    use.card = wrapped;

    QVariant data = QVariant::fromValue(use);
    RoomThread *thread = room->getThread();
    thread->trigger(PreCardUsed, room, use.from, data);
    use = data.value<CardUseStruct>();

    LogMessage log;
    log.from = use.from;
    log.to = use.to;
    log.type = "#UseCard";
    log.card_str = toString();
    room->sendLog(log);

    CardMoveReason reason(CardMoveReason::S_REASON_USE, use.from->objectName(), use.to.first()->objectName(), this->getSkillName(), QString());
    room->moveCardTo(this, use.to.first(), Player::PlaceDelayedTrick, reason, true);

    thread->trigger(CardUsed, room, use.from, data);
    use = data.value<CardUseStruct>();
    thread->trigger(CardFinished, room, use.from, data);
}

void DelayedTrick::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    QStringList nullified_list = room->getTag("CardUseNullifiedList").toStringList();
    bool all_nullified = nullified_list.contains("_ALL_TARGETS");
    if (all_nullified || targets.isEmpty()) {
        if (movable) {
            onNullified(source);
            if (room->getCardOwner(getEffectiveId()) != source) return;
        }
        CardMoveReason reason(CardMoveReason::S_REASON_USE, source->objectName(), QString(), this->getSkillName(), QString());
        room->moveCardTo(this, room->getCardOwner(getEffectiveId()), NULL, Player::DiscardPile, reason, true);
    }
}

QString DelayedTrick::getSubtype() const
{
    return "delayed_trick";
}

void DelayedTrick::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();

    CardMoveReason reason(CardMoveReason::S_REASON_USE, effect.to->objectName(), getSkillName(), QString());
    room->moveCardTo(this, NULL, Player::PlaceTable, reason, true);

    LogMessage log;
    log.from = effect.to;
    log.type = "#DelayedTrick";
    log.arg = effect.card->objectName();
    room->sendLog(log);

    JudgeStruct judge_struct = judge;
    judge_struct.who = effect.to;
    room->judge(judge_struct);

    if (judge_struct.isBad()) {
        takeEffect(effect.to);
        if (room->getCardOwner(getEffectiveId()) == NULL) {
            CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, QString());
            room->throwCard(this, reason, NULL);
        }
    }
    else if (movable) {
        onNullified(effect.to);
    }
    else {
        if (room->getCardOwner(getEffectiveId()) == NULL) {
            CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, QString());
            room->throwCard(this, reason, NULL);
        }
    }
}

void DelayedTrick::onNullified(ServerPlayer *target) const
{
    Room *room = target->getRoom();
    RoomThread *thread = room->getThread();
    if (movable) {
        QList<ServerPlayer *> players = room->getOtherPlayers(target);
        players << target;
        ServerPlayer *p = NULL;

        foreach(ServerPlayer *player, players) {
            if (player->containsTrick(objectName()))
                continue;

            const ProhibitSkill *skill = room->isProhibited(target, player, this);
            if (skill) {
                if (skill->isVisible()) {
                    LogMessage log;
                    log.type = "#SkillAvoid";
                    log.from = player;
                    log.arg = skill->objectName();
                    log.arg2 = objectName();
                    room->sendLog(log);

                    room->broadcastSkillInvoke(skill->objectName());
                }
                continue;
            }

            CardMoveReason reason(CardMoveReason::S_REASON_TRANSFER, target->objectName(), QString(), this->getSkillName(), QString());
            room->moveCardTo(this, target, player, Player::PlaceDelayedTrick, reason, true);

            if (target == player) break;

            CardUseStruct use;
            use.from = NULL;
            use.to << player;
            use.card = this;
            QVariant data = QVariant::fromValue(use);
            thread->trigger(TargetConfirming, room, player, data);
            CardUseStruct new_use = data.value<CardUseStruct>();
            if (new_use.to.isEmpty()) {
                p = player;
                break;
            }

            foreach(ServerPlayer *p, room->getAllPlayers())
                thread->trigger(TargetConfirmed, room, p, data);
            break;
        }
        if (p)
            onNullified(p);
    }
    else {
        CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, target->objectName());
        room->throwCard(this, reason, NULL);
    }
}

Weapon::Weapon(Suit suit, int number, int range)
    : EquipCard(suit, number), range(range)
{
    can_recast = true;
}

bool Weapon::isAvailable(const Player *player) const
{
    QString mode = player->getGameMode();
    if (mode == "04_1v3" && !player->isCardLimited(this, Card::MethodRecast))
        return true;
    return !player->isCardLimited(this, Card::MethodUse) && EquipCard::isAvailable(player);
}

int Weapon::getRange() const
{
    return range;
}

QString Weapon::getSubtype() const
{
    return "weapon";
}

void Weapon::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;
    ServerPlayer *player = card_use.from;
    if (room->getMode() == "04_1v3"
        && use.card->isKindOf("Weapon")
        && (player->isCardLimited(use.card, Card::MethodUse)
        || (!player->getHandPile().contains(getEffectiveId())
        && player->askForSkillInvoke("weapon_recast", QVariant::fromValue(use))))) {
        CardMoveReason reason(CardMoveReason::S_REASON_RECAST, player->objectName());
        reason.m_eventName = "weapon_recast";
        room->moveCardTo(use.card, player, NULL, Player::DiscardPile, reason);
        player->broadcastSkillInvoke("@recast");

        LogMessage log;
        log.type = "#UseCard_Recast";
        log.from = player;
        log.card_str = use.card->toString();
        room->sendLog(log);

        player->drawCards(1, "weapon_recast");
        return;
    }
    EquipCard::onUse(room, use);
}

EquipCard::Location Weapon::location() const
{
    return WeaponLocation;
}

QString Weapon::getCommonEffectName() const
{
    return "weapon";
}

QString Armor::getSubtype() const
{
    return "armor";
}

EquipCard::Location Armor::location() const
{
    return ArmorLocation;
}

QString Armor::getCommonEffectName() const
{
    return "armor";
}

Horse::Horse(Suit suit, int number, int correct)
    : EquipCard(suit, number), correct(correct)
{
}

int Horse::getCorrect() const
{
    return correct;
}

void Horse::onInstall(ServerPlayer *) const
{
}

void Horse::onUninstall(ServerPlayer *) const
{
}

QString Horse::getCommonEffectName() const
{
    return "horse";
}

OffensiveHorse::OffensiveHorse(Card::Suit suit, int number, int correct)
    : Horse(suit, number, correct)
{
}

QString OffensiveHorse::getSubtype() const
{
    return "offensive_horse";
}

DefensiveHorse::DefensiveHorse(Card::Suit suit, int number, int correct)
    : Horse(suit, number, correct)
{
}

QString DefensiveHorse::getSubtype() const
{
    return "defensive_horse";
}

EquipCard::Location Horse::location() const
{
    if (correct > 0)
        return DefensiveHorseLocation;
    else
        return OffensiveHorseLocation;
}

QString Treasure::getSubtype() const
{
    return "treasure";
}

EquipCard::Location Treasure::location() const
{
    return TreasureLocation;
}

QString Treasure::getCommonEffectName() const
{
    return "treasure";
}

//mapo tofu
MapoTofu::MapoTofu(Card::Suit suit, int number)
    : BasicCard(suit, number)
{
    setObjectName("mapo_tofu");
}

QString MapoTofu::getSubtype() const
{
    return "food_card";
}

bool MapoTofu::IsAvailable(const Player *player, const Card *tofu)
{
    MapoTofu *newanaleptic = new MapoTofu(Card::NoSuit, 0);
    newanaleptic->deleteLater();
#define THIS_TOFU (tofu == NULL ? newanaleptic : tofu)
    if (player->isCardLimited(THIS_TOFU, Card::MethodUse) || player->isProhibited(player, THIS_TOFU))
        return false;

    return player->usedTimes("MapoTofu") <= Sanguosha->correctCardTarget(TargetModSkill::Residue, player, THIS_TOFU);
#undef THIS_ANALEPTIC
}

bool MapoTofu::isAvailable(const Player *player) const
{

    return IsAvailable(player, this) && BasicCard::isAvailable(player);
}

bool MapoTofu::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.length() == 0 && Self->distanceTo(to_select) <= 1 && to_select->getMark("mtUsed") == 0;
}

void MapoTofu::onUse(Room *room, const CardUseStruct &card_use) const
{
    CardUseStruct use = card_use;
    if (use.to.isEmpty())
        use.to << use.from;
    BasicCard::onUse(room, use);
}

void MapoTofu::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    if (targets.isEmpty())
        targets << source;
    BasicCard::use(room, source, targets);
}

void MapoTofu::onEffect(const CardEffectStruct &effect) const
{
    Room *room = effect.to->getRoom();
    //room->setEmotion(effect.to, "mapo_tofu");//TODO

    DamageStruct damage;
    damage.to = effect.to;
    damage.damage = effect.to->getHp() > 0 ? effect.to->getHp() - 1: 0;
    int toDamge = damage.damage;
    // damage.chain = false;
    damage.chain = true;
    damage.nature = DamageStruct::Fire;
    effect.to->getRoom()->damage(damage);
    LogMessage log;
    log.type = "#MapoTofuUse";
    log.from = effect.from;
    log.to << effect.to;
    log.arg = objectName();
    room->sendLog(log);
    effect.to->setMark("mtUsed", toDamge + 1);
}



YinshenCard::YinshenCard()
{
    target_fixed = true;
}

void YinshenCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const
{
    room->akarinPlayer(source);
}

class Yinshen : public ZeroCardViewAsSkill
{
public:
    Yinshen() : ZeroCardViewAsSkill("yinshen")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("YinshenCard");
    }

    const Card *viewAs() const
    {
        return new YinshenCard;
    }
};

class Keji : public TriggerSkill
{
public:
    Keji() : TriggerSkill("keji")
    {
        events << PreCardUsed << CardResponded << EventPhaseChanging;
        frequency = Frequent;
        global = true;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *lvmeng, QVariant &data) const
    {
        if (triggerEvent == EventPhaseChanging) {
            bool can_trigger = true;
            if (lvmeng->hasFlag("KejiSlashInPlayPhase")) {
                can_trigger = false;
                lvmeng->setFlags("-KejiSlashInPlayPhase");
            }
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::Discard && lvmeng->isAlive() && lvmeng->hasSkill(this)) {
                if (can_trigger && lvmeng->askForSkillInvoke(this)) {
                    if (lvmeng->getHandcardNum() > lvmeng->getMaxCards()) {
                        int index = qrand() % 2 + 1;
                        if (!lvmeng->hasInnateSkill(this) && lvmeng->hasSkill("mouduan"))
                            index += 4;
                        else if (Player::isNostalGeneral(lvmeng, "lvmeng"))
                            index += 2;
                        room->broadcastSkillInvoke(objectName(), index);
                    }
                    lvmeng->skip(Player::Discard);
                }
            }
        }
        else if (lvmeng->getPhase() == Player::Play) {
            const Card *card = NULL;
            if (triggerEvent == PreCardUsed)
                card = data.value<CardUseStruct>().card;
            else
                card = data.value<CardResponseStruct>().m_card;
            if (card->isKindOf("Slash"))
                lvmeng->setFlags("KejiSlashInPlayPhase");
        }

        return false;
    }
};

//akarin
class SE_Touming : public TriggerSkill
{
public:
    SE_Touming() : TriggerSkill("SE_Touming")
    {
        events << EventPhaseStart << EventPhaseEnd << Death;
        frequency = NotFrequent;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *akarin, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (!akarin->hasSkill(objectName()))
                return false;
            if (akarin->getPhase() == Player::Discard)
                akarin->setMark("SE_Touming_num", akarin->getHandcardNum());
            else if (akarin->getPhase() == Player::RoundStart && akarin->getMark("touming_used") > 0){
                room->removeAkarinEffect(akarin);
                akarin->setMark("touming_used", 0);
            }
        }
        else if (triggerEvent == EventPhaseEnd)
        {
            if (!akarin->hasSkill(objectName()))
                return false;
            if (akarin->getPhase() == Player::Discard && akarin->getHandcardNum() == akarin->getMark("SE_Touming_num"))
            {
                if (!akarin->askForSkillInvoke(objectName(), data))
                    return false;
                room->broadcastSkillInvoke(objectName());
                room->doLightbox("SE_Touming$", 1500);
                room->akarinPlayer(akarin);
                akarin->setMark("touming_used", 1);
                akarin->drawCards((room->getAlivePlayers().length() + 1)/2);
            }
        }
        else if (triggerEvent == Death){
            DeathStruct death = data.value<DeathStruct>();
            if (death.who != akarin)
                return false;
            room->removeAkarinEffect(akarin);
        }

        return false;
    }
};

class SE_ToumingClear : public DetachEffectSkill
{
public:
    SE_ToumingClear() : DetachEffectSkill("SE_Touming")
    {
    }

    void onSkillDetached(Room *room, ServerPlayer *player) const
    {
        room->removeAkarinEffect(player);
    }
};

class SE_Tuanzi : public TriggerSkill
{
public:
    SE_Tuanzi() : TriggerSkill("SE_Tuanzi")
    {
        events << CardUsed;
        frequency = NotFrequent;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *akarin, QVariant &data) const
    {
        if (triggerEvent == CardUsed) {
            if (akarin->getPhase() == Player::Play)
            {
                CardUseStruct use = data.value<CardUseStruct>();
                if ((use.card->isKindOf("TrickCard") && use.card->isBlack()) || use.card->isKindOf("BasicCard"))
                {
                    if (!akarin->askForSkillInvoke(objectName(), data))
                        return false;
                    room->broadcastSkillInvoke(objectName());
                    QList<int> ids;
                    ids.append(use.card->getEffectiveId());
                    CardsMoveStruct move(ids, NULL, Player::DrawPile,
                        CardMoveReason(CardMoveReason::S_REASON_PUT, akarin->objectName(), objectName(), QString()));
                    room->moveCardsAtomic(move, true);
                }
            }
        }

        return false;
    }
};

class Huanxing : public TriggerSkill
{
public:
    Huanxing() : TriggerSkill("huanxing")
    {
        events << CardUsed << EventPhaseEnd << TurnStart << TrickCardCanceling << SlashProceed;
        frequency = NotFrequent;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *nao, QVariant &data) const
    {
        if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.to.length() == 0 || !use.to.at(0) || use.to.at(0)->getMark("disappear") == 1 || use.from->objectName() == use.to.at(0)->objectName())
                return false;
            if (use.to.length() > 1)
                return false;
            if (!use.to.at(0)->hasSkill(objectName()))
                return false;
            if (!use.to.at(0)->askForSkillInvoke(objectName(), data))
                return false;
            foreach(ServerPlayer* p, room->getAlivePlayers()){
                if (p->getMark("@huanxing_target") > 0){
                    p->loseMark("@huanxing_target");
                    room->removeAkarinEffect(use.to.at(0), p);
                }
            }
            use.from->gainMark("@huanxing_target");
            room->broadcastSkillInvoke(objectName(), rand() % 4 + 1);
            room->doLightbox("huanxing$", 300);
            room->akarinPlayer(use.to.at(0), use.from);
            use.to.at(0)->setMark("disappear", 1);
            return true;
        }
        else if (triggerEvent == EventPhaseEnd) {
            if (!nao || !nao->hasSkill(objectName()) || nao->getPhase() != Player::Finish)
                return false;
            foreach(ServerPlayer* p, room->getAlivePlayers()){
                if (p->getMark("@huanxing_target") > 0){
                    p->loseMark("@huanxing_target");
                    room->removeAkarinEffect(nao, p);
                }
            }

        }
        else if (triggerEvent == TurnStart) {
            if (!nao || !nao->hasSkill(objectName()))
                return false;
            nao->setMark("disappear", 0);

        }
        else if (triggerEvent == TrickCardCanceling) {
            CardEffectStruct effect = data.value<CardEffectStruct>();
            if (effect.from && effect.from->hasSkill(objectName()) && effect.to && effect.to->getMark("@huanxing_target") == 1){
                LogMessage log;
                log.type = "#huanxing_effect";
                log.from = effect.to;
                log.arg = effect.card->objectName();
                room->sendLog(log);
                room->broadcastSkillInvoke(objectName(), 6);
                return true;
            }
        }
        else if (triggerEvent == SlashProceed) {
            SlashEffectStruct effect = data.value<SlashEffectStruct>();
            if (effect.from && effect.from->hasSkill(objectName()) && effect.to && effect.to->getMark("@huanxing_target") == 1){
                LogMessage log;
                log.type = "#huanxing_effect";
                log.from = effect.to;
                log.arg = effect.slash->objectName();
                room->sendLog(log);
                room->broadcastSkillInvoke(objectName(), 5);
                room->slashResult(effect, NULL);
                return true;
            }
        }

        return false;
    }
    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

class Fushang : public TriggerSkill
{
public:
    Fushang() : TriggerSkill("fushang")
    {
        events << Damaged << EventPhaseStart << EnterDying;
        frequency = NotFrequent;
    }

    void doFushang(Room *room, ServerPlayer *player) const
    {
        if (player->getMark("@fushang_time") > 0){
            player->loseMark("@fushang_time");
            if (player->getMark("@fushang_time") == 0){
                room->broadcastSkillInvoke(objectName(), 1);
                room->recover(player, RecoverStruct(player, NULL, player->getMark("@fushang")));
                player->loseAllMarks("@fushang");
            }
        }
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {

        if (triggerEvent == Damaged) {
            DamageStruct damage = data.value<DamageStruct>();
            if (!damage.to || damage.to->isDead()){
                return false;
            }
            ServerPlayer *nao = room->findPlayerBySkillName(objectName());
            if (!nao || !nao->askForSkillInvoke(objectName(), data))
                return false;
            room->broadcastSkillInvoke(objectName());
            damage.to->gainMark("@fushang");
            damage.to->gainMark("@fushang_time", 2 - damage.to->getMark("@fushang_time"));
            return false;
        }
        else if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::RoundStart){
                doFushang(room, player);
            }
        }
        else if (triggerEvent == EnterDying){
            if (player->containsTrick("key_trick")){
                doFushang(room, player);
            }
        }
        return false;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

KeyTrick::KeyTrick(Card::Suit suit, int number)
    : DelayedTrick(suit, number)
{
    setObjectName("key_trick");
    mute = true;
    handling_method = Card::MethodNone;
}

bool KeyTrick::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    int count=0;
    int key=0;
    QList<const Player *> sib = Self->getAliveSiblings();
    sib << Self;
    foreach (const Player *p, sib){
        if(p->hasClub()&&p->getClubName()=="yanjubu"){
             count=count+1;
        }
    }
    foreach (const Card *c, to_select->getJudgingArea()){
        if(c->objectName()==objectName()){
             key=key+1;
        }
    }
    if (targets.isEmpty() && (key==0 ||(key<count && to_select->hasClub()&&to_select->getClubName()=="yanjubu")))
        return true;
    /*if (!targets.isEmpty() || to_select->containsTrick(objectName()))
        return false;*/
    return false;
}

void KeyTrick::takeEffect(ServerPlayer *) const
{
}

void KeyTrick::onEffect(const CardEffectStruct &) const
{
}

void KeyTrick::onNullified(ServerPlayer *player) const
{
    player->getRoom()->throwCard(this, NULL, player);
}

void KeyTrick::onUse(Room *room, const CardUseStruct &card_use) const
{
    DelayedTrick::onUse(room, card_use);
}


class GuangyuViewAsSkill : public OneCardViewAsSkill
{
public:
    GuangyuViewAsSkill() : OneCardViewAsSkill("guangyu")
    {
        response_pattern = "@@guangyu";
    }

    bool viewFilter(const Card *to_select) const
    {
        QStringList guangyu = Self->property("guangyu").toString().split("+");
        foreach(QString id, guangyu) {
            bool ok;
            if (id.toInt(&ok) == to_select->getEffectiveId() && ok)
                return true;
        }
        return false;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        KeyTrick *gy = new KeyTrick(originalCard->getSuit(), originalCard->getNumber());
        gy->addSubcard(originalCard);
        gy->setSkillName("guangyu");
        return gy;
    }
};

class Guangyu : public TriggerSkill
{
public:
    Guangyu() : TriggerSkill("guangyu")
    {
        events << BeforeCardsMove;
        view_as_skill = new GuangyuViewAsSkill;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (move.from != player)
            return false;
        if (move.to_place == Player::DiscardPile
            && ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD)) {

            int i = 0;
            QList<int> guangyu_card;
            foreach(int card_id, move.card_ids) {
                const Card *c = Sanguosha->getCard(card_id);
                if (room->getCardOwner(card_id) == move.from && c->isRed()) {
                    guangyu_card << card_id;
                }
                i++;
            }
            if (guangyu_card.isEmpty())
                return false;

            room->setPlayerProperty(player, "guangyu", IntList2StringList(guangyu_card).join("+"));
            if (room->getTag("nagisa_voice").isNull())
                room->setTag("nagisa_voice", QVariant(1));
            int num = room->getTag("nagisa_voice").toInt();

            do {
                if (!room->askForUseCard(player, "@@guangyu", "@guangyu-use")) break;
                if (num > 40)
                    room->broadcastSkillInvoke(objectName(), 40);
                else
                    room->broadcastSkillInvoke(objectName(), num);
                num++;
                QList<int> ids = StringList2IntList(player->property("guangyu").toString().split("+"));
                QList<int> to_remove;
                foreach(int card_id, guangyu_card) {
                    if (!ids.contains(card_id))
                        to_remove << card_id;
                }
                move.removeCardIds(to_remove);
                data = QVariant::fromValue(move);
                guangyu_card = ids;
            } while (!guangyu_card.isEmpty());
            room->setTag("nagisa_voice", QVariant(num));
        }
        return false;
    }
};

class GuangyuTrigger : public TriggerSkill
{
public:
    GuangyuTrigger() : TriggerSkill("#guangyu-trigger")
    {
        events << EventPhaseStart << PreCardUsed;
        frequency = NotFrequent;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (!player || player->getPhase() != Player::Judge || player->getJudgingArea().length() == 0)
                return false;
            foreach(const Card* card, player->getJudgingArea()){
                if (card->isKindOf("KeyTrick")){
                    ServerPlayer *nagisa = room->findPlayerBySkillName("guangyu");
                    if (!nagisa || !nagisa->askForSkillInvoke("guangyu", data))
                        return false;
                    int num = room->getTag("nagisa_voice").toInt();
                    if (num > 40)
                        room->broadcastSkillInvoke("guangyu", rand()%9 + 31);
                    else
                        room->broadcastSkillInvoke("guangyu", num);
                    num++;
                    room->setTag("nagisa_voice", QVariant(num));
                    room->doLightbox("guangyu$", 800);
                    foreach(const Card* card, player->getJudgingArea()){
                        player->obtainCard(card);
                    }
                    return false;
                }
            }
        }
        else if (triggerEvent == PreCardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("KeyTrick") && use.card->getSkillName() == "guangyu") {
                QList<int> ids = StringList2IntList(player->property("guangyu").toString().split("+"));
                ids.removeOne(use.card->getEffectiveId());
                room->setPlayerProperty(player, "guangyu", IntList2StringList(ids).join("+"));
            }
            return false;
        }
        return false;
    }
    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

class Xiyuan : public TriggerSkill
{
public:
    Xiyuan() : TriggerSkill("xiyuan")
    {
        events << Death;
        frequency = NotFrequent;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == Death) {
            DeathStruct death = data.value<DeathStruct>();

            if (death.who != player)
                return false;
            if (!death.who->hasSkill(objectName()) || room->getOtherPlayers(death.who).length() == 0)
                return false;
            if (room->getTag("xiyuan_used").toBool() || !death.who->askForSkillInvoke(objectName(), data))
                return false;
            ServerPlayer *tomoya = room->askForPlayerChosen(death.who, room->getOtherPlayers(death.who), objectName());
            room->broadcastSkillInvoke(objectName());
            room->doLightbox("xiyuan$", 3000);
            room->changeHero(tomoya, "Ushio", false, true, true, true);
            LogMessage log;
            log.type = "#XiyuanChangeHero";
            log.from = death.who;
            log.to << tomoya;
            log.arg = objectName();
            room->sendLog(log);
            room->setTag("xiyuan_used", QVariant(true));
        }
        return false;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->hasSkill(this);
    }
};

class Chengmeng : public TriggerSkill
{
public:
    Chengmeng() : TriggerSkill("chengmeng")
    {
        frequency = Club;
        club_name = "yanjubu",
        events << CardsMoveOneTime << TurnStart;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
         if (triggerEvent == CardsMoveOneTime) {
             CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
             ServerPlayer *to;
             if (!move.to)
                 return false;
             foreach(ServerPlayer *p, room->getAlivePlayers()){
                 if (p->objectName() == move.to->objectName())
                     to = p;
             }
             if (room->getCurrent()&&(room->getCurrent()->getPhase() == Player::Draw||room->getCurrent()->getPhase()==Player::NotActive)){
                 return false;
             }
             ServerPlayer *current = room->getCurrent();
             if (!current)
                 return false;
             if (!to)
                 return false;
             if (to == player)
                 return false;
             if (move.to_place!= Player::PlaceHand)
                 return false;
             if (to->hasClub() || player->getMark("chengmeng_used")>0 || !player->askForSkillInvoke(objectName(), data))
                 return false;
             room->setPlayerMark(player,"chengmeng_used",1);
             if (room->askForChoice(to, "chengmeng", "chengmeng_accept+cancel", QVariant::fromValue(player)) == "chengmeng_accept"){
                 to->addClub("yanjubu");
             }
             else{
                 LogMessage log;
                 log.type = "$refuse_club";
                 log.from = to;
                 log.arg = "yanjubu";
                 room->sendLog(log);
             }
         }
         else if (triggerEvent == TurnStart) {
             if (player->getMark("chengmeng_used")>0)
                 room->setPlayerMark(player,"chengmeng_used",0);
         }
         return false;
    }
};

class Dingxin : public TriggerSkill
{
public:
    Dingxin() : TriggerSkill("dingxin")
    {
        events << EventPhaseStart << Dying;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart) {
            if (!player || player->getPhase() != Player::RoundStart || !player->hasSkill(objectName()))
                return false;
            if (room->getTag("nagisa_voice").isNull())
                room->setTag("nagisa_voice", QVariant(1));
            if (player->getHp() > 1){
                int num = room->getTag("nagisa_voice").toInt();
                if (num > 16)
                    room->broadcastSkillInvoke(objectName(), rand() % 5 + 12);
                else
                    room->broadcastSkillInvoke(objectName(), num);
                room->setTag("nagisa_voice", QVariant(num + 1));
            }

            player->setFlags("dingxin_used");
            room->loseHp(player);
            player->setFlags("-dingxin_used");
        }
        else if (triggerEvent == Dying) {
            DyingStruct dying = data.value<DyingStruct>();
            if (!dying.who->hasSkill(objectName()) || !dying.who->hasFlag("dingxin_used"))
                return false;
            ServerPlayer *nagisa;
            foreach(ServerPlayer *p, room->getPlayers()){
                if ((p->getGeneralName() == "Nagisa" || (p->getGeneral2() && p->getGeneral2Name() == "Nagisa")) && p->isDead()){
                    nagisa = p;
                    break;
                }
            }
            QString choice;
            if (nagisa && nagisa->isDead()){
                choice = room->askForChoice(dying.who, objectName(), "dingxin_recover+dingxin_revive", data);
            }
            else{
                choice = "dingxin_recover";
            }
            if (choice == "dingxin_recover"){
                room->broadcastSkillInvoke(objectName(), rand()%2 + 17);
                room->doLightbox("dingxin$", 2000);
                room->recover(dying.who, RecoverStruct(dying.who, NULL, 3));
                LogMessage log;
                log.type = "#DingxinRecover";
                log.from = dying.who;
                room->sendLog(log);
            }
            else{
                room->broadcastSkillInvoke(objectName(), 19);
                room->doLightbox("dingxin$", 2000);
                room->revivePlayer(nagisa, true);
                room->setPlayerProperty(nagisa, "hp", QVariant(2));
                nagisa->drawCards(2);
                LogMessage log;
                log.type = "#DingxinRevive";
                log.from = dying.who;
                log.to << nagisa;
                room->sendLog(log);
            }
            return false;
        }
        return false;
    }
};

//Dark Sakura
class Xushu : public TriggerSkill
{
public:
    Xushu() : TriggerSkill("xushu")
    {
        frequency = Compulsory;
        events << Predamage << EventPhaseStart;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == Predamage){
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from->hasSkill(objectName()) || damage.to->hasSkill(objectName())) {
                if (damage.from->hasSkill(objectName(), 1)){
                    if (damage.reason != "shengjian_black")
                        room->broadcastSkillInvoke(objectName());
                    room->sendCompulsoryTriggerLog(damage.from, objectName());
                }
                else{
                    if (damage.to->getHp() > 4)
                        room->broadcastSkillInvoke(objectName(), 2);
                    room->sendCompulsoryTriggerLog(damage.to, objectName());
                }
                room->loseHp(damage.to, damage.damage);

                return true;
            }
        }
        else if (triggerEvent == EventPhaseStart){
            if (!player->hasSkill(objectName()) || player->getPhase() != Player::RoundStart)
                return false;
            room->loseHp(room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName()));
            room->broadcastSkillInvoke(objectName(), rand()%2 + 3);
        }

        return false;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

//xishou
class Xishou : public TriggerSkill
{
public:
    Xishou() : TriggerSkill("xishou")
    {
        frequency = Frequent;
        events << Dying;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == Dying){
            DyingStruct dying = data.value<DyingStruct>();
            ServerPlayer *sakura = room->findPlayerBySkillName(objectName());
            if (!sakura || dying.who == sakura || !player->hasSkill(objectName()))
                return false;
            QList<const Skill *> list = dying.who->getVisibleSkillList();
            QStringList choices;
            foreach(const Skill *skill, list){
                if (!sakura->hasSkill(skill))
                    choices.append(skill->objectName());
            }
            if (choices.length() == 0 || !sakura->askForSkillInvoke(objectName(), data))
                return false;

            QString choice = room->askForChoice(sakura, objectName(), choices.join("+"), data);
            room->broadcastSkillInvoke(objectName());
            if (!sakura->hasSkill(choice))
                room->acquireSkill(sakura, choice);
            room->recover(sakura, RecoverStruct(sakura));
        }

        return false;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

//shengbei
//Dark Sakura
class Shengbei : public TriggerSkill
{
public:
    Shengbei() : TriggerSkill("shengbei")
    {
        frequency = Compulsory;
        events << DrawNCards << TurnStart;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == DrawNCards){
            if (player->hasSkill(objectName())){
                data.setValue(data.toInt() + 3);
            }
        }
        else if (triggerEvent == TurnStart){
            if (player->hasSkill(objectName())){
                bool do_voice = true;
                if (!player->faceUp()){
                    room->broadcastSkillInvoke(objectName());
                    player->turnOver();
                    do_voice = false;
                }
                if (player->getJudgingArea().length() > 0){
                    foreach(const Card* card, player->getJudgingArea()){
                        room->throwCard(card, player);
                    }
                    if (do_voice)
                        room->broadcastSkillInvoke(objectName());
                }
            }
        }
        return false;
    }
};

class ShengbeiMaxCards : public MaxCardsSkill
{
public:
    ShengbeiMaxCards() : MaxCardsSkill("#shengbei")
    {
    }

    int getFixed(const Player *target) const
    {
        if (target->hasSkill("shengbei"))
            return target->getHp() + 3;
        else
            return -1;
    }
};

//caoying
class Caoying : public TriggerSkill
{
public:
    Caoying() : TriggerSkill("caoying")
    {
        frequency = Frequent;
        events << TargetConfirmed << HpLost;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == TargetConfirmed){
            CardUseStruct use = data.value<CardUseStruct>();
            foreach(ServerPlayer *p, use.to){
                if (p->hasSkill(objectName()) && p == player && !use.from->hasSkill(objectName())){
                    use.from->gainMark("@kage");
                }
            }
        }
        else if (triggerEvent == HpLost){
            if (player->getMark("@kage") == 0)
                return false;
            ServerPlayer *sakura = room->findPlayerBySkillName(objectName());
            if (!sakura)
                return false;
            if (sakura->askForSkillInvoke(objectName(), data)){
                room->broadcastSkillInvoke(objectName());
                for (int i = 0; i < player->getMark("@kage"); i++){
                    if (!player->isNude()){
                        room->throwCard(room->askForCardChosen(sakura, player, "he", objectName()), player, sakura);
                    }
                    else{
                        break;
                    }
                }
                player->loseAllMarks("@kage");
            }
        }

        return false;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

class ShengjianBlack : public TriggerSkill
{
public:
    ShengjianBlack() : TriggerSkill("shengjian_black")
    {
        frequency = Frequent;
        events << HpLost;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == HpLost){
            if (player->hasSkill(objectName()) && player->askForSkillInvoke(objectName(), data)){
                ServerPlayer *p = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName());
                if (!p)
                    return false;
                room->broadcastSkillInvoke(objectName());
                DamageStruct damage;
                damage.from = player;
                damage.to = p;
                damage.reason = "shengjian_black";
                damage.damage = abs(player->getEquips().length() - p->getEquips().length());
                room->damage(damage);
                foreach(const Card* card, p->getEquips()){
                    room->throwCard(card, p, player);
                }
            }
        }
        return false;
    }
};


class Fengbi : public TriggerSkill
{
public:
    Fengbi() : TriggerSkill("fengbi")
    {
        frequency = Compulsory;
        events << NonTrigger;
    }

    bool trigger(TriggerEvent , Room *, ServerPlayer *, QVariant &) const
    {
        return false;
    }
};

//chuangzao
class Chuangzao : public TriggerSkill
{
public:
    Chuangzao() : TriggerSkill("chuangzao")
    {
        frequency = Frequent;
        events << CardUsed << EventPhaseStart;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardUsed){
            CardUseStruct use = data.value<CardUseStruct>();
            if (!use.card || !use.card->isKindOf("TrickCard") || !use.from || !use.from->hasSkill(objectName()))
                return false;
            if (use.from->getMark("@Chuangzao_trick_used") == 0)
                use.from->gainMark("@Chuangzao_trick_used");
        }
        else if (triggerEvent == EventPhaseStart){
            if (player->getPhase() == Player::Discard){
                if (player->getMark("@Chuangzao_trick_used") == 0){
                    if (!player->askForSkillInvoke(objectName(), data))
                        return false;
                    player->drawCards(1);
                    int id = room->askForCardChosen(player, player, "he", objectName(), true);
                    if (id == -1)
                        return false;
                    player->addToPile("music", id, true);
                }
                else{
                    player->loseAllMarks("@Chuangzao_trick_used");
                }
            }
        }
        return false;
    }
};

QidaoCard::QidaoCard()
{
}

bool QidaoCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const
{
    return targets.length() == 0;
}

void QidaoCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.at(0);
    if (!target)
        return;
    int id = room->askForCardChosen(source, target, "h", "qidao");
    room->showCard(target, id);
    const Card *card = Sanguosha->getCard(id);
    CardUseStruct use;
    Slash *slash = new Slash(Card::NoSuit, 0);
    switch (card->getSuit())
    {
    case Card::Spade:
        use.from = source;
        use.to.append(target);
        use.card = slash;
        room->useCard(use, false);
        break;
    case Card::Heart:
        room->recover(source, RecoverStruct(source));
        break;
    case Card::Diamond:
        source->drawCards(2);
        break;
    case Card::Club:
        room->askForDiscard(target, objectName(), 2, 2, false, true);
        break;
    default:
        break;
    }
}
class Qidao : public OneCardViewAsSkill
{
public:
    Qidao() : OneCardViewAsSkill("qidao"){

    }

    bool viewFilter(const Card *) const
    {
        return true;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        QidaoCard *qdc = new QidaoCard();
        qdc->addSubcard(originalCard);
        qdc->setSkillName("qidao");
        return qdc;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("QidaoCard");
    }
};

class Benpao : public TriggerSkill
{
public:
    Benpao() : TriggerSkill("benpao")
    {
        frequency = Wake;
        events << EventPhaseStart << EventPhaseChanging;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart){
            if (player->getMark("@waked") == 1 || !player->hasSkill(objectName()))
                return false;
            if (player->getPile("music").length() < 3)
                return false;
            player->drawCards(room->getAlivePlayers().length());
            room->acquireSkill(player, "guangmang");
            room->acquireSkill(player, "shuohuang");
            player->setMark("benpao_turn", 1);
            player->gainMark("@waked");
        }
        else if (triggerEvent == EventPhaseChanging){
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::NotActive && player->getMark("benpao_turn") == 1){
                player->setMark("benpao_turn", 0);
                player->gainAnExtraTurn();
            }
        }
        return false;
    }
};

class Guangmang : public TriggerSkill
{
public:
    Guangmang() : TriggerSkill("guangmang")
    {
        frequency = NotFrequent;
        events << EventPhaseStart;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart){
            if (player->getPile("music").length() == 0 || !player->askForSkillInvoke(objectName(), data))
                return false;
            ServerPlayer *target = room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName());
            if (!target)
                return false;
            QVariant newdata;
            newdata.setValue(target);
            QString choice = room->askForChoice(player, objectName(), "guangmang_hp+guangmang_handcards", newdata);
            if (choice == "guangmang_hp")
                room->setPlayerProperty(target, "hp", player->getHp());
            else{
                if (target->getHandcardNum() > player->getHandcardNum())
                    room->askForDiscard(target, objectName(), target->getHandcardNum() - player->getHandcardNum(), target->getHandcardNum() - player->getHandcardNum());
                else
                    target->drawCards(player->getHandcardNum() - target->getHandcardNum());
            }
        }
        return false;
    }
};

ShuohuangCard::ShuohuangCard()
{
}

bool ShuohuangCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return targets.length() == 0 && (to_select->getWeapon() || (to_select->getArmor() && to_select != Self));
}

void ShuohuangCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.at(0);
    if (!target)
        return;
    QString choice;
    if (target->getWeapon() && target->getArmor() && target != source){
        QVariant newdata;
        newdata.setValue(target);
        choice = room->askForChoice(source, objectName(), "shuohuang_weapon+shuohuang_armor", newdata);
    }
    else if (target->getWeapon()){
        choice = "shuohuang_weapon";
    }
    if (choice == "shuohuang_weapon"){
        room->throwCard(target->getWeapon(), target, source);
        source->drawCards(1);
    }
    else{
        if (source->getArmor())
            room->throwCard(source->getArmor(), source, source);
        CardsMoveStruct move;
        move.card_ids.append(target->getArmor()->getEffectiveId());
        move.from = target;
        move.to = source;
        move.from_place = Player::PlaceEquip;
        move.to_place = Player::PlaceEquip;
        move.reason = CardMoveReason(CardMoveReason::S_REASON_CHANGE_EQUIP, source->objectName());
        room->moveCardsAtomic(move, true);
    }
}

class Shuohuang : public OneCardViewAsSkill
{
public:
    Shuohuang() : OneCardViewAsSkill("shuohuang"){

    }

    bool viewFilter(const Card *to_select) const
    {
        return to_select->isKindOf("BasicCard");
    }

    const Card *viewAs(const Card *originalCard) const
    {
        ShuohuangCard *shc = new ShuohuangCard();
        shc->addSubcard(originalCard);
        shc->setSkillName("shuohuang");
        return shc;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("ShuohuangCard");
    }
};

//shana rework
//zhena
class Zhena : public TriggerSkill
{
public:
    Zhena() : TriggerSkill("Zhena")
    {
        frequency = NotFrequent;
        events << DamageCaused;
    }
    int getPriority(TriggerEvent) const
    {
        return -2;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (triggerEvent == DamageCaused){
            if (damage.nature != DamageStruct::Fire || !damage.from->hasSkill(objectName()) || damage.from->getPhase() != Player::Play || damage.from->hasFlag("zhena_used") || !damage.from->askForSkillInvoke(objectName(), data))
                return false;
            room->broadcastSkillInvoke(objectName());
            room->doLightbox("Zhena$", 2500);

            damage.from->setFlags("zhena_used");

            damage.damage += damage.to->getHp();
            data.setValue(damage);

            if (damage.from->getHp() > 1)
                room->loseHp(damage.from, damage.from->getHp() - 1);

            
        }
        return false;
    }
};

class Tianhuo : public TriggerSkill
{
public:
    Tianhuo() : TriggerSkill("Tianhuo")
    {
        frequency = Compulsory;
        events << DamageCaused << DamageInflicted;
    }
    int getPriority(TriggerEvent) const
    {
        return 2;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (triggerEvent == DamageInflicted){
            if (damage.to->hasSkill(objectName()) && damage.nature == DamageStruct::Fire && damage.to->isAlive()){
                room->broadcastSkillInvoke(objectName(), 2);
                damage.to->drawCards(damage.to->getLostHp());
                return true;
            }
        }
        else{
            if (!damage.card->isKindOf("Slash") && !damage.card->isKindOf("Duel") || !damage.from->hasSkill(objectName())){
                return false;
            }
            damage.nature = DamageStruct::Fire;
            data.setValue(damage);
        }

        return false;
    }
};

//nanami
class Shengyou : public TriggerSkill
{
public:
    Shengyou() : TriggerSkill("shengyou")
    {
        frequency = NotFrequent;
        events << EventPhaseStart;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart){
            if (!player || !player->isAlive())
                return false;
            if (player->getPhase() == Player::RoundStart && player->getHandcardNum() < 3 && player->hasSkill(objectName())){
                QStringList people_list = Sanguosha->getLimitedGeneralNames();
                foreach (ServerPlayer *p, room->getAlivePlayers()){
                    if (people_list.contains(p->getGeneralName()))
                        people_list.removeOne(p->getGeneralName());
                    if (people_list.contains(p->getGeneral2Name()))
                        people_list.removeOne(p->getGeneral2Name());
                }
                foreach(QString name, people_list){
                    if (Sanguosha->getGeneral(name)->isMale())
                        people_list.removeOne(name);
                }
                //special
                if (people_list.contains("Louise"))
                    people_list.removeOne("Louise");
                if (people_list.contains("Misaka_Imouto"))
                    people_list.removeOne("Misaka_Imouto");
                if (people_list.contains("Natsume_Rin"))
                    people_list.removeOne("Natsume_Rin");
                if (people_list.contains("Riko"))
                    people_list.removeOne("Riko");
                if (people_list.contains("Koishi"))
                    people_list.removeOne("Koishi");
                if (people_list.contains("mianma"))
                    people_list.removeOne("mianma");
                if (people_list.contains("tsukushi"))
                    people_list.removeOne("tsukushi");
                if (people_list.contains("Niko"))
                    people_list.removeOne("Niko");
                if (people_list.length() > 0){
                    if (!player->askForSkillInvoke(objectName()))
                        return false;
                    QString general = room->askForGeneral(player, people_list.join("+"));
                    if (general == "")
                        return false;
                    room->broadcastSkillInvoke(objectName());
                    room->doLightbox("shengyou$", 800);
                    if (player->getGeneralName() == "Nanami"){
                        room->changeHero(player, general, false, false, false, true);
                        room->setTag("shengyou_isSecond", QVariant(false));
                    }
                    else{
                        room->changeHero(player, general, false, false, true, true);
                        room->setTag("shengyou_isSecond", QVariant(true));
                    }
                    room->attachSkillToPlayer(player, objectName());
                }
            }
            else if (player->getPhase() == Player::Finish){
                if (room->getTag("shengyou_isSecond").isNull() || player->getGeneralName() == "Nanami" || player->getGeneral2Name() == "Nanami")
                    return false;
                room->detachSkillFromPlayer(player, objectName());
                room->changeHero(player, "Nanami", false, false, room->getTag("shengyou_isSecond").toBool(), true);
            }
        }
        return false;
    }
};

class Jinqu : public TriggerSkill
{
public:
    Jinqu() : TriggerSkill("jinqu")
    {
        frequency = NotFrequent;
        events << DamageInflicted;
    }
    int getPriority(TriggerEvent) const
    {
        return -3;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.to == player && damage.to->hasSkill(objectName())){
            if (!player->askForSkillInvoke(objectName(), data)){
                return false;
            }
            room->broadcastSkillInvoke(objectName());
            player->turnOver();
            player->drawCards(player->getLostHp() * damage.damage * 2);
            QList<int> list;
            foreach(const Card *card, player->getHandcards()){
                list.append(card->getEffectiveId());
            }
            while (room->askForYiji(player, list, objectName(), false, false, true, -1, room->getOtherPlayers(player))) {
                list.clear();
                foreach(const Card *card, player->getHandcards()){
                    list.append(card->getEffectiveId());
                }
                if (!player->isAlive())
                    return false;
            }
            if (player->faceUp() && player->getJudgingArea().length() > 0){
                room->throwCard(room->askForCardChosen(player, player, "j", objectName()), player, player);
            }
        }
        return false;
    }
};

//tomoya
ZhurenCard::ZhurenCard()
{
    will_throw = false;
}

bool ZhurenCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return to_select != Self && targets.length() == 0;
}

void ZhurenCard::use(Room *room, ServerPlayer *player, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.at(0);
    if (!target)
        return;
    player->tag["zhurenCardNum"] = QVariant::fromValue(this->subcardsLength());
    room->obtainCard(target, this, false);
}

class Zhuren : public ViewAsSkill
{
public:
    Zhuren() : ViewAsSkill("zhuren")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("ZhurenCard");
    }

    bool viewFilter(const QList<const Card *> &selected, const Card *) const
    {
        int key_num = 0;
        foreach(const Card *card, Self->getJudgingArea())
            key_num += card->isKindOf("KeyTrick") ? 1 : 0;

        return selected.length() < Self->getLostHp() + key_num;
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty())
            return NULL;
        ZhurenCard *zrc = new ZhurenCard();
        zrc->addSubcards(cards);
        return zrc;
    }
};

class ZhurenTrigger : public TriggerSkill
{
public:
    ZhurenTrigger() : TriggerSkill("#zhuren")
    {
        frequency = NotFrequent;
        events << EventPhaseEnd;
    }

    bool trigger(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseEnd){
            if (player->isAlive() && player->hasSkill("zhuren") && player->getPhase() == Player::Discard && player->tag.contains("zhurenCardNum")){
                int card_num = player->tag["zhurenCardNum"].toInt();
                if (card_num > 0)
                    player->drawCards(card_num);
                player->tag.remove("zhurenCardNum");
            }

        }
        return false;
    }
};


class Daolu : public TriggerSkill
{
public:
    Daolu() : TriggerSkill("Daolu")
    {
        frequency = Wake;
        events << AskForPeachesDone;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == AskForPeachesDone){
            DyingStruct dying = data.value<DyingStruct>();
            if (dying.who != player || !player->hasSkill(objectName()) || player->getMark("@Nagisa") > 0 || player->getMark("@Tomoyo") > 0 || player->getMark("@Fuko") > 0 || player->getMark("@Kyou") > 0)
                return false;
            QString choice = room->askForChoice(player, objectName(), "Nagisa_Protector+Kyou_Lover+Tomoyo_Couple+Fuko_summoner");
            room->loseMaxHp(player);
            room->setPlayerProperty(player, "hp", QVariant(2));
            if (choice == "Nagisa_Protector"){
                room->broadcastSkillInvoke(objectName(), 1);
                room->doLightbox("DaoluA$", 3000);
                player->gainMark("@Nagisa");
                if (!player->hasSkill("diangong")){
                    room->acquireSkill(player, "diangong");
                    room->acquireSkill(player, "#diangong");
                }
            }
            else if (choice == "Tomoyo_Couple"){
                room->broadcastSkillInvoke(objectName(), 3);
                room->doLightbox("DaoluB$", 3000);
                player->gainMark("@Tomoyo");
                if (!player->hasSkill("shouyang")){
                    room->acquireSkill(player, "shouyang");
                }
            }
            else if (choice == "Kyou_Lover"){
                room->broadcastSkillInvoke(objectName(), 2);
                room->doLightbox("DaoluD$", 3000);
                player->gainMark("@Kyou");
                if (!player->hasSkill("tanyan")){
                    room->acquireSkill(player, "tanyan");
                }
            }
            else{
                room->broadcastSkillInvoke(objectName(), 4);
                room->doLightbox("DaoluC$", 3000);
                player->gainMark("@Fuko");
                if (!player->hasSkill("haixing")){
                    room->acquireSkill(player, "haixing");
                }
            }
        }
        return false;
    }
};

DiangongCard::DiangongCard()
{
    will_throw = false;
}

bool DiangongCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const
{
    /*
    foreach(const Card *card, to_select->getJudgingArea()){
        if (card->isKindOf("Lightning"))
            return false;
    }
    */
    return targets.length() == 0 ;
}

void DiangongCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.at(0);
    if (!target)
        return;
    Lightning *l = new Lightning(this->getSuit(), this->getNumber());
    l->addSubcard(this);
    l->setSkillName("diangong");
    CardUseStruct use;
    use.from = source;
    use.to.append(target);
    use.card = l;
    bool toJudge = target->containsTrick("lightning");
    room->useCard(use, true);
    if (toJudge){
        JudgeStruct judge;
        judge.pattern = ".|spade|2~9";
        judge.good = false;
        judge.reason = objectName();
        judge.time_consuming = true;
        judge.who = target;
        judge.negative = true;
        room->judge(judge);
        if (judge.isEffected()){
            room->damage(DamageStruct(l, NULL, target, 3, DamageStruct::Thunder));
            CardMoveReason reason(CardMoveReason::S_REASON_NATURAL_ENTER, QString());
            room->throwCard(l, reason, NULL);
        }
    }
}

class Diangong : public OneCardViewAsSkill
{
public:
    Diangong() : OneCardViewAsSkill("diangong"){

    }

    bool viewFilter(const Card *card) const
    {
        return card->isBlack() && !card->isEquipped();
    }

    const Card *viewAs(const Card *originalCard) const
    {
        DiangongCard *dgc = new DiangongCard();
        dgc->addSubcard(originalCard);
        dgc->setSkillName("diangong");
        return dgc;
    }

    bool isEnabledAtPlay(const Player *) const
    {
        return true;
    }
};

class DiangongTrigger : public TriggerSkill
{
public:
    DiangongTrigger() : TriggerSkill("#diangong")
    {
        frequency = NotFrequent;
        events << DamageInflicted;
    }
    int getPriority(TriggerEvent) const
    {
        return -3;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (damage.card && (damage.card->isKindOf("Lighting") || damage.card->getSkillName() == "diangong")){
            ServerPlayer *tomoya = room->findPlayerBySkillName("diangong");
            if (!tomoya)
                return false;
            if (tomoya->askForSkillInvoke("diangongDamage", data)){
                room->broadcastSkillInvoke("diangong");
                ServerPlayer *toRecover = room->askForPlayerChosen(tomoya, room->getAlivePlayers(), objectName(), "@diangong-from");
                room->recover(toRecover, RecoverStruct(toRecover));
                return true;
            }
        }
        return false;
    }
    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

class Shouyang : public TriggerSkill
{
public:
    Shouyang() : TriggerSkill("shouyang")
    {
        frequency = Compulsory;
        events << DamageInflicted << Death << EventAcquireSkill;
    }
    int getPriority(TriggerEvent) const
    {
        return -4;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventAcquireSkill && data.toString() == objectName()){
            room->askForPlayerChosen(player, room->getOtherPlayers(player), objectName(), "@Daolu-Tomo")->gainMark("@Tomo");
        }
        else if (triggerEvent == DamageInflicted){
            DamageStruct damage = data.value<DamageStruct>();

            ServerPlayer *tomoya = room->findPlayerBySkillName(objectName());
            if (tomoya && damage.to == tomoya){
                foreach(ServerPlayer *p, room->getAlivePlayers()){
                    if (p->getMark("@Tomo") > 0){
                        p->drawCards(2);
                    }
                }
                return false;
            }


            if (damage.to->getMark("@Tomo") == 0){
                return false;
            }

            if (!tomoya)
                return false;
            damage.to = tomoya;
            room->broadcastSkillInvoke(objectName());
            LogMessage log;
            log.type = "#shouyangTrigger";
            log.from = tomoya;
            log.to << damage.to;
            room->sendLog(log);
            data.setValue(damage);
        }
        else if (triggerEvent == Death){

            if (player->hasSkill(objectName())){
                foreach(ServerPlayer *p, room->getAlivePlayers()){
                    if (p->getMark("@Tomo") > 0){
                        DummyCard *dummy = new DummyCard(player->handCards());
                        QList <const Card *> equips = player->getEquips();
                        foreach(const Card *card, equips)
                            dummy->addSubcard(card);

                        if (dummy->subcardsLength() > 0) {
                            CardMoveReason reason(CardMoveReason::S_REASON_RECYCLE, p->objectName());
                            room->obtainCard(p, dummy, reason, false);
                        }
                        delete dummy;
                        return false;
                    }
                }
            }
        }

        return false;
    }
    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

class ShouyangClear : public DetachEffectSkill
{
public:
    ShouyangClear() : DetachEffectSkill("shouyang")
    {
    }

    void onSkillDetached(Room *room, ServerPlayer *player) const
    {
        foreach(ServerPlayer *p, room->getAlivePlayers()){
            p->loseAllMarks("@Tomo");
        }
    }
};

class Haixing : public TriggerSkill
{
public:
    Haixing() : TriggerSkill("haixing")
    {
        frequency = NotFrequent;
        events << Dying;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == Dying){
            DyingStruct dying = data.value<DyingStruct>();
            if (!player || !player->hasSkill(objectName()) || !player->askForSkillInvoke(objectName(), data) || !room->askForDiscard(player, objectName(), 1, 1))
                return false;
            room->broadcastSkillInvoke(objectName());
            JudgeStruct judge;
            judge.pattern = ".";
            judge.reason = objectName();
            judge.who = player;
            judge.time_consuming = true;
            room->judge(judge);
            if (judge.card->getNumber() > 8)
                room->recover(dying.who, RecoverStruct(dying.who));
            if (judge.card->isRed())
                room->recover(dying.who, RecoverStruct(dying.who));
        }

        return false;
    }
};

class Tanyan : public TriggerSkill
{
public:
    Tanyan() : TriggerSkill("tanyan")
    {
        frequency = NotFrequent;
        events << EventPhaseStart << EventPhaseEnd << CardFinished;
    }
    int getPriority(TriggerEvent) const
    {
        return -2;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::Play){
            ServerPlayer *tomoya = room->findPlayerBySkillName(objectName());
            if (!tomoya || tomoya->isKongcheng() || !tomoya->askForSkillInvoke(objectName()))
                return false;
            room->broadcastSkillInvoke(objectName());
            room->showAllCards(tomoya, player);
            player->setFlags("tanyan_target");
            room->setFixedDistance(player, tomoya, 1);
        }
        else if (triggerEvent == EventPhaseEnd && player->getPhase() == Player::Play){
            if (player->hasFlag("tanyan_target")){
                player->setFlags("-tanyan_target");
                ServerPlayer *tomoya = room->findPlayerBySkillName(objectName());
                if (!tomoya)
                    return false;
                room->removeFixedDistance(player, tomoya, 1);
                player->setMark("tanyan_slash", 0);
            }
        }
        else if (triggerEvent == CardFinished && player->getPhase() == Player::Play){
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.from == player && use.card->isKindOf("Slash") && player->hasFlag("tanyan_target") && player->getMark("tanyan_slash") < 2){
                ServerPlayer *tomoya = room->findPlayerBySkillName(objectName());
                if (!tomoya)
                    return false;
                tomoya->obtainCard(use.card);
                player->obtainCard(Sanguosha->getCard(room->askForCardChosen(tomoya, tomoya, "he", objectName())));
                player->setMark("tanyan_slash", player->getMark("tanyan_slash") + 1);
            }
        }

        return false;
    }
    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && !target->isMale();
    }
};

class Pasheng : public DistanceSkill
{
public:
    Pasheng() : DistanceSkill("SE_Pasheng")
    {
    }

    int getCorrect(const Player *from, const Player *to) const
    {
        if (from->hasSkill(this))
            return 100;
        else if (to->hasSkill(this))
            return -100;
        else
            return 0;
    }
};

class Maoqun : public TriggerSkill
{
public:
    Maoqun() : TriggerSkill("SE_Maoqun")
    {
        frequency = Compulsory;
        events << Damage;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &) const
    {
        if (triggerEvent == Damage){
            ServerPlayer *rin = room->findPlayerBySkillName("SE_Maoqun");
            if (!rin)
                return false;
            room->broadcastSkillInvoke(objectName());
            room->loseHp(rin);
            if (room->getDrawPile().length() == 0)
                room->swapPile();
            rin->addToPile("Neko", room->getDrawPile().at(0));
        }
        return false;
    }
    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

class MaoqunHeg : public TriggerSkill
{
public:
    MaoqunHeg() : TriggerSkill("SE_MaoqunHeg")
    {
        frequency = Compulsory;
        events << GameStart << EventAcquireSkill;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == GameStart || (triggerEvent == EventAcquireSkill && data.toString() == objectName())){
            room->broadcastSkillInvoke(objectName());
            for (int i = 0; i < room->getAlivePlayers().count(); i++){
                if (room->getDrawPile().length() == 0)
                    room->swapPile();
                player->addToPile("Neko", room->getDrawPile().at(0));
            }
           
        }
        return false;
    }
};

class Chengzhang : public TriggerSkill
{
public:
    Chengzhang() : TriggerSkill("SE_Chengzhang")
    {
        frequency = Wake;
        events << EventPhaseStart;
    }
    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::RoundStart && player->getMark("@waked") == 0 && player->getPile("Neko").length() >= room->getAlivePlayers().length() * 3 / 2){
            if (player->getMaxHp() >= 99)
                room->loseMaxHp(player, 96);
            else
                room->loseMaxHp(player, player->getMaxHp() - 3);
            room->broadcastSkillInvoke(objectName());
            player->gainMark("@waked");
            room->doLightbox("SE_Chengzhang$", 3000);
            room->detachSkillFromPlayer(player, "SE_Pasheng");
            room->detachSkillFromPlayer(player, "SE_Maoqun");
            room->acquireSkill(player, "zhiling");
            room->acquireSkill(player, "#zhiling");
            room->acquireSkill(player, "#zhiling-max");
            room->acquireSkill(player, "SE_Zhixing");
        }
        return false;
    }
};

ZhilingCard::ZhilingCard()
{
}

bool ZhilingCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *) const
{
    return targets.length() == 0 && (to_select->getMark("@Neko_S") == 0 || to_select->getMark("@Neko_C") == 0 || to_select->getMark("@Neko_D") == 0 || to_select->getMark("@Neko_H") == 0) && !to_select->hasFlag("Can_not");
}

void ZhilingCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.at(0);
    if (!target)
        return;
    QList<int> list = source->getPile("Neko");
    QList<int> left = source->getPile("Neko");
    if (target->getMark("@Neko_S") > 0){
        foreach(int id, list){
            if (Sanguosha->getCard(id)->getSuit() == Card::Spade)
                left.removeOne(id);
        }
    }
    if (target->getMark("@Neko_C") > 0){
        foreach(int id, list){
            if (Sanguosha->getCard(id)->getSuit() == Card::Club)
                left.removeOne(id);
        }
    }
    if (target->getMark("@Neko_D") > 0){
        foreach(int id, list){
            if (Sanguosha->getCard(id)->getSuit() == Card::Diamond)
                left.removeOne(id);
        }
    }
    if (target->getMark("@Neko_H") > 0){
        foreach(int id, list){
            if (Sanguosha->getCard(id)->getSuit() == Card::Heart)
                left.removeOne(id);
        }
    }
    if (left.length() == 0){
        room->setPlayerFlag(target, "Can_not");
        return;
    }
    room->fillAG(left, source);
    int id = room->askForAG(source, left, false, objectName());
    room->clearAG(source);
    if (id == -1)
        return;
    switch (Sanguosha->getCard(id)->getSuit()){
    case Card::Spade:
        target->gainMark("@Neko_S");
        break;
    case Card::Club:
        target->gainMark("@Neko_C");
        break;
    case Card::Diamond:
        target->gainMark("@Neko_D");
        break;
    case Card::Heart:
        target->gainMark("@Neko_H");
        break;
    }
    room->throwCard(id, NULL, NULL);
}

class Zhiling : public ZeroCardViewAsSkill
{
public:
    Zhiling() : ZeroCardViewAsSkill("zhiling")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return player->getPile("Neko").length() > 0;
    }

    const Card *viewAs() const
    {
        return new ZhilingCard;
    }
};

class ZhilingTrigger : public TriggerSkill
{
public:
    ZhilingTrigger() : TriggerSkill("#zhiling")
    {
        frequency = Compulsory;
        events << DrawNCards << DamageInflicted << AskForPeaches;
    }
    bool trigger(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == DrawNCards && player->getMark("@Neko_S") > 0){
            if (rand() % 3 == 0)
                data.setValue(data.toInt() - 2);
        }
        else if (triggerEvent == DamageInflicted){
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.nature != DamageStruct::Normal && damage.to->getMark("@Neko_D") > 0){
                damage.damage += 1;
                data.setValue(damage);
            }
        }
        else if (triggerEvent == AskForPeaches){
            DyingStruct dying = data.value<DyingStruct>();
            if (dying.who->getMark("@Neko_H") > 0 && rand() % 2 == 1)
                return dying.who->getSeat() != player->getSeat();
        }

        return false;
    }
    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

class ZhilingMaxCards : public MaxCardsSkill
{
public:
    ZhilingMaxCards() : MaxCardsSkill("#zhiling-max")
    {
    }

    int getExtra(const Player *target) const
    {
        if (target->getMark("@Neko_C") > 0)
            return -1;
        else
            return 0;
    }
};

class Zhixing : public TriggerSkill
{
public:
    Zhixing() : TriggerSkill("SE_Zhixing")
    {
        frequency = NotFrequent;
        events << Dying << DamageInflicted;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == Dying){
            DyingStruct dying = data.value<DyingStruct>();
            if (!player || !player->hasSkill(objectName()))
                return false;
            foreach(const Card* card, dying.who->getJudgingArea()){
                if (card->isKindOf("KeyTrick"))
                    return false;
            }
            QVariant newData;
            newData.setValue(dying.who);
            if (!player->askForSkillInvoke(objectName(), newData)){
                return false;
            }
            room->broadcastSkillInvoke(objectName());
            room->doLightbox("SE_Zhixing$", 800);
            QList<ServerPlayer*> players = room->getAlivePlayers();
            foreach(ServerPlayer* p, players){
                if (p->isNude() && p->getJudgingArea().length() == 0)
                    players.removeOne(p);
            }
            if (players.length() == 0)
                return false;
            ServerPlayer *from = room->askForPlayerChosen(player, players, objectName(), "@zhixing-from");
            if (!from)
                return false;
            int id = room->askForCardChosen(player, from, "hej", objectName());
            if (id == -1)
                return false;
            KeyTrick *key = new KeyTrick(Sanguosha->getCard(id)->getSuit(), Sanguosha->getCard(id)->getNumber());
            key->addSubcard(id);
            key->setSkillName(objectName());
            CardUseStruct use;
            use.from = player;
            use.to.append(dying.who);
            use.card = key;
            room->useCard(use, true);
        }
        else if (triggerEvent == DamageInflicted){
            DamageStruct damage = data.value<DamageStruct>();
            bool hasKey = false;
            int id = -1;
            foreach(const Card* card, damage.to->getJudgingArea()){
                if (card->isKindOf("KeyTrick")){
                    hasKey = true;
                    id = card->getEffectiveId();
                }
            }
            if (!hasKey)
                return false;
            QVariant newData;
            newData.setValue(damage.to);
            ServerPlayer *rin = room->findPlayerBySkillName(objectName());
            if (!rin || !rin->askForSkillInvoke(objectName(), newData))
                return false;
            room->broadcastSkillInvoke(objectName());
            room->doLightbox("SE_Zhixing$", 800);
            room->throwCard(id, damage.to, rin);
            return true;
        }

        return false;
    }
    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

//koromo
class Kongdi : public TriggerSkill
{
public:
    Kongdi() : TriggerSkill("kongdi")
    {
        frequency = NotFrequent;
        events << CardsMoveOneTime;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime){
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            ServerPlayer *koromo = room->findPlayerBySkillName(objectName());
            if (!koromo || !move.to || koromo->getHandcardNum() >= move.to->getHandcardNum() - move.card_ids.length() || koromo == move.to || move.to_place != Player::PlaceHand || !move.from_places.contains(Player::DrawPile) || !koromo->askForSkillInvoke(objectName(), data))
                return false;
            ServerPlayer *to;
            foreach(ServerPlayer *p, room->getAlivePlayers()){
                if (p->objectName() == move.to->objectName())
                    to = p;
            }
            if (!to)
                return false;
            int id = room->askForCardChosen(koromo, to, "h", objectName(), true);
            if (id == -1)
                return false;
            room->showCard(to, id);
            QString choice = room->askForChoice(koromo, objectName(), "kongdi_di+kongdi_discard");
            if (rand() % 5 == 1){
                room->broadcastSkillInvoke(objectName());
            }
            if (choice == "kongdi_di"){
                CardsMoveStruct move;
                move.card_ids.append(id);
                move.to_place = Player::DrawPileBottom;
                move.reason.m_reason = CardMoveReason::S_REASON_PUT;
                room->moveCardsAtomic(move, false);
            }
            else{
                room->throwCard(id, to, koromo);
            }
        }
        return false;
    }
};

class Yixiangting : public TriggerSkill
{
public:
    Yixiangting() : TriggerSkill("yixiang")
    {
        frequency = Compulsory;
        events << BeforeCardsMove;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == BeforeCardsMove){
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            ServerPlayer *koromo = room->findPlayerBySkillName(objectName());
            if (!koromo || !move.to || koromo == move.to || move.to_place != Player::PlaceHand || !move.from_places.contains(Player::DrawPile))
                return false;
            QList<int> new_ids;
            QList<int> to_remove;
            int rd;
            foreach(int id, move.card_ids){
                if (room->getDrawPile().contains(id)){
                    rd = rand() % (room->getDrawPile().length());
                    while (new_ids.contains(room->getDrawPile().at(rd)))
                        rd = rand() % (room->getDrawPile().length());
                    new_ids.append(room->getDrawPile().at(rd));
                    to_remove.append(id);
                }
            }
            move.removeCardIds(to_remove);
            foreach(int new_id, new_ids){
                move.card_ids.append(new_id);
                move.from_places.append(Player::DrawPile);
                move.from_pile_names.append(NULL);
                move.open.append(false);
            }
            if (move.to->getPhase() != Player::Draw && rand() % 3 == 1){
                room->broadcastSkillInvoke(objectName());
            }
            data.setValue(move);
        }
        return false;
    }
};

//kyou

class TouzhiVS : public OneCardViewAsSkill
{
public:
    TouzhiVS() : OneCardViewAsSkill("touzhi")
    {
    }

    bool isEnabledAtPlay(const Player *) const
    {
        return true;
    }

    bool viewFilter(const Card *card) const
    {
        if (!card->isKindOf("TrickCard") || card->isKindOf("AOE") || card->isKindOf("GodSalvation") || card->isKindOf("AmazingGrace") || card->isKindOf("Collateral"))
            return false;
        return true;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        Card *slash = new Slash(originalCard->getSuit(), originalCard->getNumber());

        slash->addSubcard(originalCard->getId());
        slash->setSkillName("touzhi");
        return slash;
    }
};

class Touzhi : public TriggerSkill
{
public:
    Touzhi() : TriggerSkill("touzhi")
    {
        events << SlashHit << SlashMissed << CardUsed;
        view_as_skill = new TouzhiVS;
    }

    int getEffectIndex(const ServerPlayer*, const Card*){
        return 0;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {if (triggerEvent == SlashHit) {
            SlashEffectStruct effect = data.value<SlashEffectStruct>();
            if (effect.slash->getSkillName() == objectName()){
                int id = effect.slash->getSubcards().at(0);
                const Card *card = Sanguosha->getCard(id);
                CardUseStruct use;
                use.from = effect.from;
                use.to.append(effect.to);
                use.card = card;
                room->broadcastSkillInvoke(objectName(), rand() % 2 + 4);
                room->useCard(use, false);
            }
        }
        else if (triggerEvent == SlashMissed) {
            SlashEffectStruct effect = data.value<SlashEffectStruct>();
            if (effect.from->hasSkill(objectName()) && effect.slash->getSkillName() == objectName()){
                room->broadcastSkillInvoke(objectName(), rand() % 3 + 1);
                Analeptic *a = new Analeptic(Card::NoSuit, 0);
                a->setSkillName(objectName());
                CardUseStruct use;
                use.from = effect.from;
                use.to.append(effect.from);
                use.card = a;
                room->useCard(use, false);
                effect.from->drawCards(1);
                effect.from->gainMark("@kyou_fire");
            }
        }
        else if (triggerEvent == CardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("Slash") && use.card->getSkillName() == objectName()) {
                if (use.m_addHistory) {
                    room->addPlayerHistory(use.from, use.card->getClassName(), -1);
                    use.m_addHistory = false;
                    data = QVariant::fromValue(use);
                }
            }
        }

        return false;
    }
};

class TouzhiDis : public DistanceSkill
{
public:
    TouzhiDis() : DistanceSkill("#touzhi")
    {
    }

    int getCorrect(const Player *from, const Player *) const
    {
        if (from->hasSkill(this))
            return - 1 - from->getMark("@kyou_fire");
        else
            return 0;
    }
};

class YoujiaoViewAsSkill : public OneCardViewAsSkill
{
public:
    YoujiaoViewAsSkill() : OneCardViewAsSkill("youjiao")
    {
        response_pattern = "@@youjiao";
    }

    bool viewFilter(const Card *to_select) const
    {
        return to_select->isKindOf("BasicCard");
    }

    const Card *viewAs(const Card *originalCard) const
    {
        KeyTrick *yj = new KeyTrick(originalCard->getSuit(), originalCard->getNumber());
        yj->addSubcard(originalCard);
        yj->setSkillName("youjiao");
        return yj;
    }
};

class Youjiao : public TriggerSkill
{
public:
    Youjiao() : TriggerSkill("youjiao")
    {
        frequency = NotFrequent;
        events << HpLost;
        view_as_skill = new YoujiaoViewAsSkill;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == HpLost){
            QVariant new_data;
            new_data.setValue(player);
            ServerPlayer *kyou = room->findPlayerBySkillName(objectName());
            if (!kyou || player->getHp() >= kyou->getHp())
                return false;
            if (room->askForUseCard(kyou, "@@youjiao", "@youjiao-use")){
                kyou->drawCards(1);
                player->drawCards(1);
            }
        }

        return false;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

class Takamakuri : public TriggerSkill
{
public:
    Takamakuri() : TriggerSkill("Takamakuri")
    {
        frequency = NotFrequent;
        events << Damage;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *akari, QVariant &data) const
    {
        if (triggerEvent == Damage){
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from != akari)
                return false;
            if (akari && akari->isAlive() && akari->askForSkillInvoke(objectName(), data)){
                akari->setFlags("TakamakuriUsed");
                int id = room->getDrawPile().at(0);
                QList<int> ids;
                ids.append(id);
                room->fillAG(ids);
                room->getThread()->delay(800);

                room->clearAG();
                if (Sanguosha->getCard(id)->isKindOf("BasicCard")){
                    room->broadcastSkillInvoke(objectName());
                    room->obtainCard(akari, id);
                    if (damage.to->getEquips().length() > 0)
                        room->throwCard(room->askForCardChosen(akari, damage.to, "e", objectName()), damage.to, akari);
                }
            }
        }
        return false;
    }
};

class Tobiugachi : public TriggerSkill
{
public:
    Tobiugachi() : TriggerSkill("Tobiugachi")
    {
        frequency = NotFrequent;
        events << CardAsked;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *akari, QVariant &data) const
    {
        if (triggerEvent == CardAsked){
            QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
            if (pattern == "jink" && akari->hasSkill(objectName()) && akari->getHandcardNum() > akari->getHp() && akari->askForSkillInvoke(objectName(), data)){
                if (room->askForDiscard(akari, objectName(), akari->getHandcardNum() - akari->getHp() + 1, akari->getHandcardNum() - akari->getHp() + 1)){
                    akari->setFlags("TobiugachiUsed");
                    Card* jink = Sanguosha->cloneCard("jink", Card::NoSuit, 0);
                    jink->setSkillName(objectName());
                    room->provide(jink);
                    ServerPlayer *target = room->askForPlayerChosen(akari, room->getAlivePlayers(), objectName());
                    QStringList list = target->getPileNames();
                    bool hasPile = false;
                    foreach (QString pile, list){
                        if (target->getPile(pile).length() > 0){
                            hasPile = true;
                        }
                    }
                    QString choice = "ToBiGetRegion";
                    if (hasPile)
                        choice = room->askForChoice(akari, objectName(), "ToBiGetRegion+TobiGetPile");
                    if (choice == "TobiGetPile"){
                        QString choice2 = room->askForChoice(akari, objectName() + "1", list.join("+"));
                        QList<int> pile = target->getPile(choice2);
                        room->fillAG(pile, akari);
                        int id = room->askForAG(akari, pile, false, objectName());
                        if (id == -1)
                            return false;
                        room->obtainCard(akari, id);
                        room->clearAG(akari);
                    }
                    else{
                        int id =  room->askForCardChosen(akari, target, "hej", objectName());
                        if (id == -1)
                            return false;
                        room->obtainCard(akari, id);
                    }
                }
            }
        }
        return false;
    }
};


class Fukurouza : public TriggerSkill
{
public:
    Fukurouza() : TriggerSkill("Fukurouza")
    {
        frequency = NotFrequent;
        events << EventPhaseEnd;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseEnd && player->getPhase() == Player::Finish){
            ServerPlayer *akari = room->findPlayerBySkillName(objectName());
            bool broad = true;
            if (akari && akari->isAlive() && akari->hasFlag("TobiugachiUsed") && room->askForSkillInvoke(akari, objectName() + "Tobi", data)){
                room->broadcastSkillInvoke(objectName());
                broad = false;
                DamageStruct damage;
                damage.from = akari;
                damage.to = player;
                damage.reason = objectName();
                room->damage(damage);
            }

            if (akari && akari->isAlive() && akari->hasFlag("TakamakuriUsed") && room->askForSkillInvoke(akari, objectName() + "Taka", data)){
                if (broad)
                    room->broadcastSkillInvoke(objectName());
                akari->drawCards(1);
                akari->setFlags("-TakamakuriUsed");
            }

            if (akari && akari->isAlive() && akari->hasFlag("TobiugachiUsed"))
                akari->setFlags("-TobiugachiUsed");
            if (akari && akari->isAlive() && akari->hasFlag("TakamakuriUsed"))
                akari->setFlags("-TakamakuriUsed");
        }
        return false;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

class Weishi : public TriggerSkill
{
public:
    Weishi() : TriggerSkill("weishi")
    {
        events << EventPhaseEnd;
        frequency = NotFrequent;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *kaga, QVariant &data) const
    {
        if (triggerEvent == EventPhaseEnd)
            {
                if (!kaga->hasSkill(objectName()))
                    return false;
                if (kaga->getPhase() == Player::Play && !kaga->isKongcheng())
                {
                    if (!kaga->askForSkillInvoke(objectName(), data))
                        return false;
                    room->broadcastSkillInvoke(objectName());
                    QList<ServerPlayer *> targets;
                    foreach(ServerPlayer *player, room->getOtherPlayers(kaga)){
                        if (player->getPileNames().length() > 0){
                            targets.append(player);
                        }
                    }
                    targets.append(kaga);
                    ServerPlayer * target = room->askForPlayerChosen(kaga, targets, objectName());
                    QStringList list = target->getPileNames();
                    if (target == kaga){
                        if (!list.contains("Kansaiki")){
                            list.append("Kansaiki");
                        }
                    }
                    QString choice = room->askForChoice(kaga, objectName(), list.join("+"));
                    int id = room->askForCardChosen(kaga, kaga, "h", objectName(), true);
                    target->addToPile(choice, id, true);
                    room->recover(target, RecoverStruct(target));
                }
            }

        return false;
    }
};


HongzhaCard::HongzhaCard()
{
    mute = true;
}

bool HongzhaCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    return to_select != Self && targets.length() < Self->getPile("Kansaiki").length() && Self->getHp() >= 2;
}

bool HongzhaCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() > 0;
}

void HongzhaCard::use(Room *room, ServerPlayer *kaga, QList<ServerPlayer *> &targets) const
{
    Card *sub = Sanguosha->getCard(this->getSubcards().at(0));
    Card *card = Sanguosha->cloneCard("slash", sub->getSuit(), sub->getNumber());
    card->setSkillName("hongzha");
    room->useCard(CardUseStruct(card, kaga, targets));
}

class Hongzha : public ViewAsSkill
{
public:
    Hongzha() : ViewAsSkill("hongzha")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("HongzhaCard") && player->getMark("@FireCaused") == 0;
    }

    bool viewFilter(const QList<const Card *> &selected, const Card *) const
    {
        return selected.length() == 0;
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty())
            return NULL;
        HongzhaCard *hzc = new HongzhaCard();
        hzc->addSubcards(cards);
        return hzc;
    }
};

class HongzhaClear : public DetachEffectSkill
{
public:
    HongzhaClear() : DetachEffectSkill("hongzha", "Kansaiki")
    {
    }
};

class Kuisi : public TriggerSkill
{
public:
    Kuisi() : TriggerSkill("kuisi")
    {
        frequency = NotFrequent;
        events << Death;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == Death){
            DeathStruct death = data.value<DeathStruct>();
            if (!death.damage || !death.damage->from || death.damage->from->isDead())
                return false;
            ServerPlayer *saki = room->findPlayerBySkillName(objectName());
            if (saki){
                if (!saki->askForSkillInvoke(objectName(), data))
                    return false;
                room->broadcastSkillInvoke(objectName());
                room->doLightbox("kuisi$", 2000);
                room->loseHp(death.damage->from, death.damage->from->getHp());
            }
        }
        return false;
    }
};

YouerCard::YouerCard()
{
    mute = true;
}

bool YouerCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) return false;
    return true;
}

void YouerCard::use(Room *room, ServerPlayer *saki, QList<ServerPlayer *> &targets) const
{
   ServerPlayer *target = targets.at(0);
   room->broadcastSkillInvoke("youer",rand() % 2+2);
   room->setPlayerMark(target, "youer_target", 1);
   foreach(ServerPlayer *p, room->getOtherPlayers(target)){
       room->setPlayerMark(p, "youer_target", 0);
   }
}

class Youervs : public ZeroCardViewAsSkill
{
public:
    Youervs() : ZeroCardViewAsSkill("youer")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("YouerCard");
    }

    const Card *viewAs() const
    {
        return new YouerCard();
    }
};

class Youer : public TriggerSkill
{
public:
    Youer() : TriggerSkill("youer")
    {
        frequency = NotFrequent;
        events << DamageCaused << EventPhaseStart;
        view_as_skill = new Youervs;
        global = true;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == DamageCaused){
            DamageStruct da = data.value<DamageStruct>();
            if (da.damage<da.to->getHp()){
                return false;
            }
            foreach(ServerPlayer *p, room->getAlivePlayers()){
                if (p&&p->getMark("youer_target")>0&&p!=da.to&&room->askForSkillInvoke(p, objectName(), data)){
                    room->broadcastSkillInvoke(objectName(),1);
                    da.to=p;
                    data.setValue(da);
                    break;
                }
            }
        }
        else if (triggerEvent == EventPhaseStart){
            if (player->getPhase()!=Player::Play||player->isKongcheng()){
                return false;
            }
            ServerPlayer *sp = NULL;
            foreach(ServerPlayer *p, room->getAlivePlayers()){
                if (p&&p->getMark("youer_target")>0&&player->inMyAttackRange(p)){
                    sp=p;
                    break;
                }
            }
            if (!sp||!sp->askForSkillInvoke("youertiaoxin",data)){
                return false;
            }
            int id = room->askForCardChosen(sp,player,"h",objectName());
            room->throwCard(id,player,sp);
            Card *c=Sanguosha->getCard(id);
            if (c->isKindOf("Slash")||c->isKindOf("Duel")){
                room->useCard(CardUseStruct(c, player, sp));
                if (c->isKindOf("Slash")) {
                    room->addPlayerHistory(player,"Slash",1);
                }
            }
        }
        return false;
    }
};

NuequCard::NuequCard()
{
    mute = true;
}

bool NuequCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) return false;
    QList<const Player *> players = Self->getAliveSiblings();
    players << Self;
    int min = 1000;
    foreach(const Player *p, players) {
        if (min > p->getHp())
            min = p->getHp();
    }
    return to_select->getHp() == min;
}

void NuequCard::use(Room *room, ServerPlayer *kongou, QList<ServerPlayer *> &targets) const
{
    Card *sub = Sanguosha->getCard(this->getSubcards().at(0));
    Card *card = Sanguosha->cloneCard("fire_slash", sub->getSuit(), sub->getNumber());
    card->setSkillName("nuequ");
    room->useCard(CardUseStruct(card, kongou, targets));
}

class Nuequ : public ViewAsSkill
{
public:
    Nuequ() : ViewAsSkill("nuequ")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("NuequCard");
    }

    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() == 0 && !to_select->isEquipped();
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty())
            return NULL;
        NuequCard *nqc = new NuequCard();
        nqc->addSubcards(cards);
        return nqc;
    }
};


class BurningLove : public TriggerSkill
{
public:
    BurningLove() : TriggerSkill("BurningLove")
    {
        frequency = NotFrequent;
        events << DamageCaused;
    }
    int getPriority(TriggerEvent) const
    {
        return -2;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *kongou, QVariant &data) const
    {
        DamageStruct damage = data.value<DamageStruct>();
        if (triggerEvent == DamageCaused){
            if (damage.from->hasSkill(objectName()) && damage.nature == DamageStruct::Fire && damage.from->isAlive() && damage.card->isKindOf("FireSlash")){
                if (room->askForSkillInvoke(kongou, objectName(), data)){
                    room->broadcastSkillInvoke(objectName());
                    room->recover(damage.to, RecoverStruct(damage.to));
                    return true;
                }
            }
        }

        return false;
    }
};


//zuikaku
EryuCard::EryuCard()
{
    mute = true;
}

bool EryuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) return false;
    return !to_select->isMale() && to_select != Self;
}

void EryuCard::use(Room *room, ServerPlayer *zuikaku, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.at(0);
    room->broadcastSkillInvoke("eryu", 1);
    target->gainMark("@EryuMark");
    zuikaku->gainMark("@EryuMark");
}


class EryuVs : public ZeroCardViewAsSkill
{
public:
    EryuVs() : ZeroCardViewAsSkill("eryu")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("EryuCard") && player->getMark("@EryuMark") == 0;
    }

    const Card *viewAs() const
    {
        return new EryuCard();
    }
};

class Eryu : public TriggerSkill
{
public:
    Eryu() : TriggerSkill("eryu")
    {
        events << CardsMoveOneTime;
        view_as_skill = new EryuVs;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *zuikaku, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime) {
            if (zuikaku->getMark("@EryuMark") == 0){
                return false;
            }
            ServerPlayer *linked = NULL;
            foreach(ServerPlayer *player, room->getOtherPlayers(zuikaku)){
                if (player->getMark("@EryuMark") > 0){
                    linked = player;
                }
            }

            if (!linked){
                return false;
            }


            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.reason.m_reason != CardMoveReason::S_REASON_USE || (move.from != zuikaku && move.from != linked)){
                return false;
            }

            if (!move.to || move.to != move.from){

                if (move.from == zuikaku){
                    bool done = false;
                    foreach(int id, move.card_ids){
                        if (id != -1 && !Sanguosha->getCard(id)->isKindOf("Nullification")){
                            linked->obtainCard(Sanguosha->getCard(id));
                            done = true;
                        }

                    }
                    if (done)
                        room->broadcastSkillInvoke("eryu", 2);
                }
                else{
                    bool done = false;
                    foreach(int id, move.card_ids){
                        if (id != -1 && !Sanguosha->getCard(id)->isKindOf("Nullification")){
                            zuikaku->obtainCard(Sanguosha->getCard(id));
                            done = true;
                        }

                    }
                    if (done)
                        room->broadcastSkillInvoke("eryu", 3);
                }
                return true;
            }
        }

        return false;
    }
};



class EryuClear : public DetachEffectSkill
{
public:
    EryuClear() : DetachEffectSkill("eryu")
    {
    }

    void onSkillDetached(Room *room, ServerPlayer *player) const
    {
        foreach(ServerPlayer *player, room->getAlivePlayers()){
            if (player->getMark("@EryuMark") > 0){
                player->loseAllMarks("@EryuMark");
            }
        }
    }
};

class Zheyi : public TriggerSkill
{
public:
    Zheyi() : TriggerSkill("zheyi")
    {
        frequency = Wake;
        events << EnterDying;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == EnterDying){
            DyingStruct dying = data.value<DyingStruct>();
            if (dying.who->getMark("@EryuMark") == 0){
                return false;
            }
            ServerPlayer *zuikaku = room->findPlayerBySkillName("zheyi");
            if (!zuikaku){
                return false;
            }
            if (zuikaku->getMark("@waked") == 1 || !zuikaku->hasSkill(objectName()))
                return false;
            room->setPlayerProperty(zuikaku, "maxhp", zuikaku->getMaxHp() + 1);
            room->broadcastSkillInvoke(objectName());
            room->doLightbox("zheyi$", 3000);
            room->detachSkillFromPlayer(zuikaku, "eryu");
            room->acquireSkill(zuikaku, "youdiz");
            room->recover(zuikaku, RecoverStruct(zuikaku));
            zuikaku->gainMark("@waked");
        }
        return false;
    }
};


class Youdiz : public TriggerSkill
{
public:
    Youdiz() : TriggerSkill("youdiz")
    {
        events << EventPhaseStart << DamageInflicted << EventPhaseEnd;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart && player->hasSkill(objectName()) && player->getPhase() == Player::RoundStart) {
            QList<ServerPlayer *> ins = QList<ServerPlayer *>();
            foreach(ServerPlayer *p, room->getOtherPlayers(player)){
                if (p->inMyAttackRange(player)){
                    ins.append(p);
                }
            }

            if (ins.length() == 0){
                return false;
            }
            if (!player->askForSkillInvoke(objectName(), data)){
                return false;
            }
            room->broadcastSkillInvoke(objectName());
            ServerPlayer *other = room->askForPlayerChosen(player, ins, objectName());
            other->gainMark("@Youdi");
            other->gainAnExtraTurn();
        }
        else if (triggerEvent == DamageInflicted){
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && damage.from->getMark("@Youdi") > 0){
                if (damage.to->hasSkill(objectName())){
                    room->askForPlayerChosen(damage.to, room->getOtherPlayers(damage.to), "youdi_draw")->drawCards(1);
                }
                else{
                    return true;
                }
            }
        }

        else if (triggerEvent == EventPhaseEnd  && player->getMark("@Youdi") > 0 && player->getPhase() == Player::Finish){
            player->loseAllMarks("@Youdi");
            player->turnOver();
        }

        return false;
    }
};


//shizuo

class Baonu : public TriggerSkill
{
public:
    Baonu() : TriggerSkill("baonu")
    {
        events << DrawNCards << EventPhaseEnd;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *shizuo, QVariant &data) const
    {
        if (triggerEvent == DrawNCards) {
            if (room->askForSkillInvoke(shizuo, objectName())){
                room->loseHp(shizuo);
                room->broadcastSkillInvoke(objectName());
                shizuo->gainMark("@Baonu");
                data.setValue(shizuo->getLostHp());
            }
        }
        else if (triggerEvent == EventPhaseEnd){
            if (shizuo->getPhase() == Player::Finish){
                shizuo->loseAllMarks("@Baonu");
            }
        }

        return false;
    }
};

JizhanCard::JizhanCard()
{
}

bool JizhanCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) return false;
    return to_select != Self && Self->inMyAttackRange(to_select) && !to_select->isNude();
}

void JizhanCard::use(Room *room, ServerPlayer *shizuo, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.at(0);
    int id = room->askForCardChosen(shizuo, target, "he", objectName());
    QList<ServerPlayer *> good_targets = room->getOtherPlayers(target);
    good_targets.removeOne(shizuo);
    ServerPlayer *target2 = room->askForPlayerChosen(shizuo, good_targets, "jizhanshiz");
    target2->obtainCard(Sanguosha->getCard(id));
    room->damage(DamageStruct(Sanguosha->getCard(id), shizuo, target2, 1));
    /*
    if (Sanguosha->getCard(id)->isKindOf("EquipCard")){
        if (target2->getEquips().length() > 0){
            room->throwCard(room->askForCardChosen(shizuo, target2, "e", objectName()), target2, shizuo);
        }
    }*/
}

class Jizhanshiz : public ZeroCardViewAsSkill
{
public:
    Jizhanshiz() : ZeroCardViewAsSkill("jizhanshiz")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("JizhanCard") && player->getMark("@Baonu") > 0;
    }

    const Card *viewAs() const
    {
        return new JizhanCard();
    }
};

//3000
class Tianzi : public TriggerSkill
{
public:
    Tianzi() : TriggerSkill("tianzi")
    {
        events << EventPhaseEnd;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *nagi, QVariant &data) const
    {
        if (triggerEvent == EventPhaseEnd){
            if (nagi->getPhase() == Player::Judge || nagi->getPhase() == Player::Draw || nagi->getPhase() == Player::Play || nagi->getPhase() == Player::Discard){
                if (nagi->isNude()){
                    return false;
                }
                const Card *card = room->askForCard(nagi, "..", "@tianzi-discard", data, objectName());
                if (card){
                    //room->throwCard(card, nagi, nagi);
                    room->broadcastSkillInvoke(objectName());
                    if (card->isKindOf("TrickCard")){
                        nagi->drawCards(2);
                    }
                    else if (card->isKindOf("EquipCard")){
                        nagi->drawCards(2);
                    }
                    else{
                        nagi->drawCards(1);
                    }
                }
            }
        }
        return false;
    }
};

class Yuzhai : public TriggerSkill
{
public:
    Yuzhai() : TriggerSkill("yuzhai")
    {
        events << EventPhaseStart << CardsMoveOneTime;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *nagi, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart){
            if (nagi->getPhase() == Player::Finish){
                if (nagi->getMark("@Yuzhai") > nagi->getHp() && room->askForSkillInvoke(nagi, objectName(), data)){
                    room->broadcastSkillInvoke(objectName());
                    for (int i = nagi->getHp(); i < nagi->getMark("@Yuzhai"); i++){
                        if (i > nagi->getHp() + 2){
                            break;
                        }
                        ServerPlayer *p = room->askForPlayerChosen(nagi, room->getOtherPlayers(nagi), objectName());
                        if (p->isNude()){
                            continue;
                        }
                        int id = room->askForCardChosen(nagi, p, "he", objectName());
                        if (id != -1){
                            room->throwCard(id, p, nagi);
                        }
                    }


                    nagi->loseAllMarks("@Yuzhai");
                }
            }
        }
        else if (triggerEvent == CardsMoveOneTime){
            if (nagi->getPhase() == Player::NotActive){
                return false;
            }
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from && move.from->hasSkill(objectName()) && nagi->objectName() == move.from->objectName() && (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD){
                nagi->gainMark("@Yuzhai", move.card_ids.length());
            }
        }
        return false;
    }
};

class Qinshi: public TriggerSkill
{
public:
    Qinshi() : TriggerSkill("qinshi")
    {
        events << GameStart << Death << EventPhaseEnd;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *mumei, QVariant &data) const
    {
        if (triggerEvent == GameStart){
            room->setPlayerProperty(mumei, "maxhp",mumei->getMaxHp() +  room->getAllPlayers().length());
            room->recover(mumei, RecoverStruct(mumei, NULL, room->getAllPlayers().length()));
        }
        else if (triggerEvent == EventPhaseEnd){
            if (mumei->getPhase() == Player::Finish){
                room->broadcastSkillInvoke(objectName());
                room->loseHp(mumei, 1);
            }
        }
        else if (triggerEvent == Death){
            DeathStruct death = data.value<DeathStruct>();
            if (!death.damage || !death.damage->from || !death.damage->from->hasSkill(objectName()))
                return false;
            room->broadcastSkillInvoke(objectName());
            room->recover(death.damage->from, RecoverStruct(death.damage->from));
        }
        return false;
    }
};

class Kangfen : public TriggerSkill
{
public:
    Kangfen() : TriggerSkill("kangfen")
    {
        events << EventPhaseEnd << Damaged;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseEnd){
            if (player->getPhase() == Player::Finish && !player->hasSkill(objectName())){
                ServerPlayer *mumei = room->findPlayerBySkillName(objectName());

                if (mumei && mumei->isAlive() && !mumei->hasFlag("kangfen_damaged")){
                    if (room->askForSkillInvoke(mumei, objectName(), data)){
                        room->broadcastSkillInvoke(objectName());
                        mumei->gainAnExtraTurn();
                    }
                }
                else{
                    if (mumei && mumei->isAlive()){
                        room->setPlayerFlag(mumei, "-kangfen_damaged");
                    }
                }
            }
        }
        else if (triggerEvent == Damaged){
            DamageStruct da = data.value<DamageStruct>();
            if (da.to && da.to->hasSkill(objectName())){
                room->setPlayerFlag(da.to, "kangfen_damaged");

            }
        }
        return false;
    }
};

class Xiedou : public ViewAsSkill
{
public:
    Xiedou() : ViewAsSkill("xiedou")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasFlag("XiedouUsed") && player->getHandcardNum() > player->getEquips().length() && Self->getEquips().length() > 0;
    }

    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() < Self->getHandcardNum() - Self->getEquips().length() && !to_select->isEquipped();
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.length() < Self->getHandcardNum() - Self->getEquips().length())
            return NULL;
        Duel *duel = new Duel(cards.at(0)->getSuit(), cards.at(0)->getNumber());
        duel->addSubcards(cards);
        Self->setFlags("XiedouUsed");
        duel->setSkillName(objectName());
        return duel;
    }
};



TaxianCard::TaxianCard()
{
}
bool TaxianCard::targetFilter(const QList<const Player *> &, const Player *to_select, const Player *Self) const
{
    return to_select != Self && Self->inMyAttackRange(to_select) && Self->canSlash(to_select);
}

bool TaxianCard::targetsFeasible(const QList<const Player *> &targets, const Player *) const
{
    return targets.length() > 0;
}

void TaxianCard::use(Room *room, ServerPlayer *ayanami, QList<ServerPlayer *> &targets) const
{
    ThunderSlash *slash = new ThunderSlash(Card::NoSuit, 0);
    if (targets.length() >= 3){
        slash->setSkillName("taxian");
    }

    room->useCard(CardUseStruct(slash, ayanami, targets));
    foreach(ServerPlayer *p , targets){
        if (p->inMyAttackRange(ayanami)){
            Slash *slash = new Slash(Card::NoSuit, 0);
            slash->setSkillName("taxian");
            room->useCard(CardUseStruct(slash, p, ayanami));
        }
    }
}

class TaxianVs : public ZeroCardViewAsSkill
{
public:
    TaxianVs() : ZeroCardViewAsSkill("taxian")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("TaxianCard");
    }

    const Card *viewAs() const
    {
        return new TaxianCard();
    }
};

class Taxian : public TriggerSkill
{
public:
    Taxian() : TriggerSkill("taxian")
    {
        events << SlashProceed;
        view_as_skill = new TaxianVs;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == SlashProceed){
            SlashEffectStruct ses = data.value<SlashEffectStruct>();
            if (ses.from && ses.from->hasSkill(objectName()) && ses.slash && ses.slash->getSkillName() == objectName()){
                room->slashResult(ses, NULL);
                return true;
            }
        }

        return false;
    }
};

class Guishen : public TriggerSkill
{
public:
    Guishen() : TriggerSkill("guishen")
    {
        events << EventPhaseEnd << Damage;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseEnd){
            if (player->getPhase() == Player::Finish){
                if (player->getMark("@Guishen") >= player->getHp()){
                    room->recover(player, RecoverStruct(player, NULL, player->getMark("@Guishen") - player->getHp()));
                    player->drawCards(player->getHp());
                }
                player->loseAllMarks("@Guishen");
            }
        }
        else if (triggerEvent == Damage){
            DamageStruct da = data.value<DamageStruct>();
            if (da.from && da.from->hasSkill(objectName()) && da.from->getPhase() != Player::NotActive){
                da.from->gainMark("@Guishen", 1);

            }
        }
        return false;
    }
};

class Fanghuo : public TriggerSkill
{
public:
    Fanghuo() : TriggerSkill("fanghuo")
    {
        events << DamageCaused << EventPhaseEnd;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == DamageCaused){
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && damage.from->hasSkill(objectName()) && damage.card && damage.card->isKindOf("Slash")){
                if (!damage.from->askForSkillInvoke(objectName(), data)){
                    return false;
                }
                room->broadcastSkillInvoke(objectName());
                damage.to->gainMark("@FireCaused");
                room->setEmotion(damage.to, "fire_caused");
            }
        }
        else if (triggerEvent == EventPhaseEnd){
            if (player->getPhase() == Player::Play && player->getMark("@FireCaused") > 0){
                room->setEmotion(player, "fire_caused");
                room->damage(DamageStruct(objectName(), player, player, 1, DamageStruct::Fire));
                if (rand() % 4 == 1){
                    player->loseMark("@FireCaused");
                }
            }
        }
        return false;
    }
};

class Jianhun : public OneCardViewAsSkill
{
public:
    Jianhun() : OneCardViewAsSkill("jianhun"){

    }

    bool viewFilter(const Card *) const
    {
        return true;
    }

    const Card *viewAs(const Card *originalCard) const
    {
        Slash *slash = new Slash(originalCard->getSuit(), originalCard->getNumber());
        slash->addSubcard(originalCard);
        slash->setSkillName(objectName());
        return slash;
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        int totalLost = 0;
        QStringList *nishimura = new QStringList();
        nishimura->append("Mogami");
        nishimura->append("Shigure");
        foreach(const Player *p, player->getSiblings()){
            if (nishimura->contains(p->getGeneralName()) || nishimura->contains(p->getGeneral2Name())){
                totalLost += p->getLostHp();
            }
        }
        totalLost += player->getLostHp();
        return totalLost >= 2 || player->getMark("@FireCaused") > 0;
    }
};

class JianhunTargetMod : public TargetModSkill
{
public:
    JianhunTargetMod() : TargetModSkill("#jianhun-target")
    {
    }

    int getDistanceLimit(const Player *from, const Card *card) const
    {
        if (from->hasSkill("jianhun") && card->getSkillName() == "jianhun")
            return 1000;
        else
            return 0;
    }

    int getResidueNum(const Player *from, const Card *card) const
    {
        if (from->hasSkill("jianhun") && card->getSkillName() == "jianhun")
            return 1000;
        else
            return 0;
    }
};


class Jianjin : public TriggerSkill
{
public:
    Jianjin() : TriggerSkill("jianjin")
    {
        events << EventPhaseStart << EventPhaseEnd << Damaged << HpRecover;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == EventPhaseStart && player->getPhase() == Player::RoundStart){
            if (player->hasSkill(objectName())){
                if (player->getMark("@Jianjin") == 0){
                    player->gainMark("@Jianjin");
                }
            }
        }
        else if (triggerEvent == EventPhaseEnd && player->getPhase() == Player::Finish){
            if (player->hasSkill(objectName())){
                if (player->getMark("@Jianjin") == 0){
                    player->gainMark("@Jianjin");
                }
            }
            ServerPlayer * iroha = room->findPlayerBySkillName(objectName());
            if (iroha && iroha->isAlive()){
                iroha->loseAllMarks("@Jianjin_damage_recovery");
            }
        }
        else if (triggerEvent == Damaged){
            DamageStruct damage = data.value<DamageStruct>();
            ServerPlayer * iroha = room->findPlayerBySkillName(objectName());
            if (iroha && iroha->getMark("@Jianjin") == 0){
                return false;
            }
            if (iroha && iroha->isAlive() && iroha->getMark("@Jianjin_damage_recovery") < 3){
                iroha->gainMark("@Jianjin_damage_recovery", damage.damage);
            }
            if ((damage.to || damage.from) && iroha && iroha->isAlive() && iroha->askForSkillInvoke(objectName(), data)){
                iroha->loseAllMarks("@Jianjin");
                QList<ServerPlayer *> sl;
                if (damage.from)
                    sl.append(damage.from);
                if (damage.to)
                    sl.append(damage.to);
                room->broadcastSkillInvoke(objectName());
                room->askForPlayerChosen(iroha, sl, objectName())->drawCards(iroha->getMark("@Jianjin_damage_recovery"));
            }
        }
        else if (triggerEvent == HpRecover){
            RecoverStruct r = data.value<RecoverStruct>();
            ServerPlayer * iroha = room->findPlayerBySkillName(objectName());
            if (!iroha || !iroha->isAlive() || iroha->getMark("@Jianjin") == 0){
                return false;
            }
            if (iroha && iroha->isAlive() && iroha->getMark("@Jianjin_damage_recovery") < 3){
                iroha->gainMark("@Jianjin_damage_recovery", r.recover);
            }
        }
        return false;
    }
};

class Faka : public TriggerSkill
{
public:
    Faka() : TriggerSkill("faka")
    {
        frequency = NotFrequent;
        events << CardsMoveOneTime << HpRecover;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime && player->hasSkill(objectName())){
            if (room->getCurrent()->getPhase() == Player::NotActive){
                return false;
            }
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            ServerPlayer *iroha = room->findPlayerBySkillName(objectName());
            if (!iroha || move.to != iroha || move.from == move.to || (move.to_place != Player::PlaceEquip && move.to_place != Player::PlaceHand)){
                return false;
            }
            ServerPlayer *current = room->getCurrent();
            if (!current || current->isDead() || current == iroha){
                return false;
            }
            if (!iroha->askForSkillInvoke(objectName(), data)){
                return false;
            }
            QList<ServerPlayer *> qs;
            qs.append(current);
            if (move.from && move.from->isAlive()){
                foreach(ServerPlayer *p, room->getAlivePlayers()){
                    if (move.from->objectName() == p->objectName()){
                        qs.append(p);
                    }
                }
            }
            ServerPlayer *goodman = room->askForPlayerChosen(iroha, qs, objectName());
            CardsMoveStruct new_move;
            new_move.card_ids = move.card_ids;
            new_move.from = iroha;
            new_move.to = goodman;
            new_move.to_place = Player::PlaceHand;
            new_move.reason.m_reason = CardMoveReason::S_REASON_GIVE;
            room->moveCardsAtomic(new_move, true);
            room->broadcastSkillInvoke(objectName());
            room->damage(DamageStruct(NULL, iroha, goodman, new_move.card_ids.length()));
        }
        else if (triggerEvent == HpRecover){
            RecoverStruct r = data.value<RecoverStruct>();
            ServerPlayer * iroha = room->findPlayerBySkillName(objectName());
            if (iroha && iroha->isAlive() && r.who == iroha){
                ServerPlayer *current = room->getCurrent();
                if (current == iroha){
                    return false;
                }
                room->broadcastSkillInvoke(objectName());
                current->turnOver();
                current->drawCards(1);

            }
        }
        return false;
    }
};



NingjuCard::NingjuCard()
{
    mute = true;
}
bool NingjuCard::targetFilter(const QList<const Player *> &, const Player *, const Player *) const
{
    return true;
}

void NingjuCard::use(Room *room, ServerPlayer *chiaki, QList<ServerPlayer *> &targets) const
{
    ServerPlayer *target = targets.at(0);
    QList<int> card_ids;
    foreach(ServerPlayer *player, room->getAlivePlayers()){
        if (player->inMyAttackRange(target)){
            player->drawCards(1);

        }
    }
    int num = 0;

    QString status = "None";
    room->setTag("ningju_color", QVariant(status));


    foreach(ServerPlayer *player, room->getAlivePlayers()){
        if (player->inMyAttackRange(target)){
            int id = room->askForCardChosen(player, player, "he", "ningju");
            if (chiaki->getMark("@waked") > 0){
                room->obtainCard(chiaki, id);
                num += 1;
            }
            else{
                if (status == "None"){
                    status = Sanguosha->getCard(id)->isRed() ? "Red" : "Black";
                }
                else if (status == "Red"){
                    status = Sanguosha->getCard(id)->isRed() ? "Red" : "Mix";
                }
                else if (status == "Black"){
                    status = Sanguosha->getCard(id)->isRed() ? "Mix" : "Black";
                }
                room->setTag("ningju_color", QVariant(status));
                room->throwCard(id, player, player);
                card_ids.append(id);
            }

        }
    }

    if (chiaki->getMark("@waked") > 0){
        status = "None";
        for (int i = 0; i < num; i++){
            int id2 = room->askForCardChosen(chiaki, chiaki, "he", "ningju");
            if (status == "None"){
                status = Sanguosha->getCard(id2)->isRed() ? "Red" : "Black";
            }
            else if (status == "Red"){
                status = Sanguosha->getCard(id2)->isRed() ? "Red" : "Mix";
            }
            else if (status == "Black"){
                status = Sanguosha->getCard(id2)->isRed() ? "Mix" : "Black";
            }
            room->setTag("ningju_color", QVariant(status));
            room->throwCard(id2, chiaki, chiaki);
            card_ids.append(id2);
        }
    }

    room->setTag("ningju_color", QVariant("None"));
    if (card_ids.length() == 0){
        return;
    }
    QList<Card::Color> colors;
    foreach(int card_id, card_ids){
        Card::Color color = Sanguosha->getCard(card_id)->getColor();
        if (!colors.contains(color)){
            colors.append(color);
        }
    }
    if (colors.length() == 1){
        Slash *slash = new Slash(Card::NoSuit, 0);
        slash->setSkillName("ningju_slash");
        room->broadcastSkillInvoke("ningju", 1);
        if (chiaki->canSlash(target, false)){
            room->doAnimate(QSanProtocol::S_ANIMATE_LIGHTBOX, "lani=skills/zhinian", QString("%1:%2").arg(1000).arg(0));
            room->useCard(CardUseStruct(slash, chiaki, target));
        }
    }
    else{
        room->broadcastSkillInvoke("ningju", 2);
    }
}


class Ningju : public ZeroCardViewAsSkill
{
public:
    Ningju() : ZeroCardViewAsSkill("ningju")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return player->usedTimes("NingjuCard") < 3;
    }

    const Card *viewAs() const
    {
        return new NingjuCard();
    }
};


class Zhinian : public TriggerSkill
{
public:
    Zhinian() : TriggerSkill("zhinian")
    {
        frequency = Wake;
        events << AskForPeachesDone;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->hasSkill(this);
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == AskForPeachesDone){
            DyingStruct dying = data.value<DyingStruct>();
            if (dying.who->hasSkill(objectName()) && dying.who->getMaxHp() > 0 && dying.who->getHp() < 1 && dying.who->getMark("@waked") == 0 && dying.who == player){
                room->broadcastSkillInvoke(objectName());
                room->doLightbox("zhinian$", 2500);
                room->setPlayerProperty(player, "hp", 3);
                player->gainMark("@waked");
                room->acquireSkill(player, "chengxu");
            }
        }
        return false;
    }
};

class Chengxu : public TriggerSkill
{
public:
    Chengxu() : TriggerSkill("chengxu")
    {
        frequency = Compulsory;
        events << DamageInflicted << EventPhaseEnd;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == DamageInflicted){
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.to->hasSkill(objectName())){
                room->broadcastSkillInvoke(objectName());
                return true;
            }
        }
        else if (triggerEvent == EventPhaseEnd){
            if (player->hasSkill(objectName()) && player->getPhase()==Player::Finish){
                room->broadcastSkillInvoke(objectName());
                room->loseMaxHp(player);
            }
        }
        return false;
    }
};


FanqianCard::FanqianCard()
{
    target_fixed = true;
}

void FanqianCard::use(Room *room, ServerPlayer *asashio, QList<ServerPlayer *> &) const
{
    QList<ServerPlayer *> all = room->getAlivePlayers();
    QStringList string;
    foreach(ServerPlayer *p, all){
        string.append(p->getGeneralName());
    }
    QString targetName = room->askForChoice(asashio, "fanqian", string.join("+"));
    ServerPlayer *target;
    foreach(ServerPlayer *p, all){
        if (p->getGeneralName() == targetName){
            target = p;
            break;
        }
    }
    if (target){
        Card *card = Sanguosha->getCard(this->subcards.at(0));
        card->setSkillName("fanqian");
        room->setTag("fanqian_target", QVariant().fromValue(target));
        CardUseStruct use = CardUseStruct(card, asashio, target);
        room->useCard(use);
    }
}


class FanqianVS : public OneCardViewAsSkill
{
public:
    FanqianVS() : OneCardViewAsSkill("fanqian")
    {
    }

    bool viewFilter(const Card *to_select) const
    {
        return !to_select->isKindOf("Collateral") && !to_select->isKindOf("Jink") && !to_select->isKindOf("Nullification") && !to_select->isKindOf("DelayedTrick");
    }

    const Card *viewAs(const Card *originalCard) const
    {
        FanqianCard *fqc = new FanqianCard();
        fqc->addSubcard(originalCard);
        fqc->setSkillName("fanqian");
        return fqc;
    }
};

class Fanqian : public TriggerSkill
{
public:
    Fanqian() : TriggerSkill("fanqian")
    {
        view_as_skill = new FanqianVS;
        events << PreCardUsed;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        if ((use.card->isKindOf("AOE") || use.card->isKindOf("GlobalEffect")) && use.card->getSkillName() == "fanqian"){
            use.to.clear();
            use.to.append(room->getTag("fanqian_target").value<ServerPlayer *>());
            data = QVariant::fromValue(use);
        }

        return false;
    }
};


class Buyu : public TriggerSkill
{
public:
    Buyu() : TriggerSkill("buyu")
    {
        events << EventPhaseStart << EventPhaseEnd << TargetConfirmed;
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *asashio, QVariant &data) const
    {
        if (event == EventPhaseStart){
            if (asashio->getPhase() == Player::Play && asashio->askForSkillInvoke(objectName(), data)){
                room->broadcastSkillInvoke(objectName(), 1);
                ServerPlayer *target = room->askForPlayerChosen(asashio, room->getAlivePlayers(), objectName());
                if (target){
                    target->gainMark("@Buyu");
                    asashio->setFlags("buyu_used");
                }
            }
        } else if(event == EventPhaseEnd){
            foreach(ServerPlayer *p, room->getAlivePlayers()){
                if (p->getMark("@Buyu") > 0)
                    p->loseAllMarks("@Buyu");
            }
        }
        else if (event == TargetConfirmed){
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.from && use.from->hasSkill(objectName()) && use.card && !(use.card->isKindOf("Slash") && use.card->isBlack())){
                foreach(ServerPlayer *p, use.to){
                    if (p->getMark("@Buyu") > 0){
                        if (!use.from->hasFlag("Buyu_sdraw_played")){
                            room->broadcastSkillInvoke(objectName(), 1);
                            use.from->setFlags("Buyu_sdraw_played");
                        }

                        use.from->drawCards(1);
                        return false;
                    }
                }
                if (!use.from->isNude() && use.from->hasFlag("buyu_used")){
                    if (!use.from->hasFlag("Buyu_sdis_played")){
                        room->broadcastSkillInvoke(objectName(), 2);
                        use.from->setFlags("Buyu_sdis_played");
                    }
                    room->askForDiscard(use.from, objectName(), 1, 1, false, true);
                }
            }
        }
        return false;
    }
};

class Xingjian : public TriggerSkill
{
public:
    Xingjian() : TriggerSkill("xingjian")
    {
        events << EventPhaseStart << Death;
        frequency = Wake;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (event == EventPhaseStart){
            if (player->getPhase() == Player::Play && player->hasSkill(objectName())){
                if (room->hasAura() && (room->getAura() == objectName() || room->getAura() == "MacrossF" || room->getAuraPlayer()->getHp() < player->getHp())){
                    return false;
                }
                if (!player->askForSkillInvoke(objectName(), data)){
                    return false;
                }
                room->broadcastSkillInvoke(objectName(), rand() % 2 * 2 + 1);
                if (room->getAura() == "yaojing"){
                    room->doAura(player, "MacrossF");
                }
                else{
                    room->doAura(player, objectName());
                }
            }
            else if (player->getPhase() == Player::RoundStart && room->hasAura() && (room->getAura() == objectName() || room->getAura() == "MacrossF") && player->getEquips().length() > 0){
                QString choice = room->askForChoice(player, objectName(), "xingjian_skip+xingjian_throw", data);
                ServerPlayer * ranka = room->findPlayerBySkillName(objectName());
                if (!ranka || player == ranka){
                    return false;
                }
                room->broadcastSkillInvoke(objectName(), 2);
                if (choice == "xingjian_throw"){

                    room->obtainCard(ranka, room->askForCardChosen(ranka, player, "e", objectName()));
                }
                else{
                    if (ranka && ranka->isAlive() && !ranka->isNude()){
                        room->obtainCard(player, room->askForCardChosen(player, ranka, "he", objectName()));
                        player->skip(Player::Draw);
                    }
                }
            }
        }
        else if (event == Death){
            DeathStruct death = data.value<DeathStruct>();
            if (death.who->hasSkill(objectName()) && room->hasAura() && (room->getAura() == objectName() || room->getAura() == "MacrossF")){
                if (room->getAura() == "MacrossF"){
                    ServerPlayer *sher = room->findPlayerBySkillName("yaojing");
                    if (sher &&sher->isAlive()){
                        room->doAura(sher, "yaojing");
                        return false;
                    }

                }
                room->clearAura();
            }
        }
        return false;
    }
};

class XingjianClear : public DetachEffectSkill
{
public:
    XingjianClear() : DetachEffectSkill("xingjian")
    {
    }

    void onSkillDetached(Room *room, ServerPlayer *player) const
    {
        if ( room->hasAura() && (room->getAura() == objectName() || room->getAura() == "MacrossF")){
            if (room->getAura() == "MacrossF"){
                ServerPlayer *sher = room->findPlayerBySkillName("yaojing");
                if (sher &&sher->isAlive()){
                    room->doAura(sher, "yaojing");
                    return;
                }

            }
            room->clearAura();
        }
    }
};

class Goutong : public TriggerSkill
{
public:
    Goutong() : TriggerSkill("goutong")
    {
        events << CardsMoveOneTime;
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (event == CardsMoveOneTime){

            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            ServerPlayer *ranka = room->findPlayerBySkillName(objectName());
            if (!ranka || (move.to != ranka && move.from != ranka) || move.from == move.to || (!move.from_places.contains(Player::PlaceHand) && !move.from_places.contains(Player::PlaceEquip)) || (move.to_place != Player::PlaceEquip && move.to_place != Player::PlaceHand)){
                return false;
            }
            ServerPlayer* from;
            ServerPlayer* to;
            foreach(ServerPlayer *p, room->getAlivePlayers()){
                if (p->objectName() == move.from->objectName()){
                    from = p;
                }
                if (p->objectName() == move.to->objectName()){
                    to = p;
                }
            }
            if (!ranka->askForSkillInvoke(objectName(), data)){
                return false;
            }
            room->broadcastSkillInvoke(objectName());
            room->recover(from, RecoverStruct(from));
            room->recover(to, RecoverStruct(to));
            from->drawCards(1);
            to->drawCards(1);

        }
        return false;
    }
};


class Jianshi : public TriggerSkill
{
public:
    Jianshi() : TriggerSkill("jianshi")
    {
        events << CardsMoveOneTime << Death;
        frequency = Compulsory;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->hasSkill(this);
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (event == CardsMoveOneTime){

            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            ServerPlayer *kotori = room->findPlayerBySkillName(objectName());
            if (!move.from_places.contains(Player::DrawPile) && !move.from_places.contains(Player::DrawPileBottom)){
                return false;
            }
            if (move.to_place == Player::DrawPile || move.to_place == Player::DrawPileBottom){
                return false;
            }

            if (!kotori || kotori->isDead()){
                return false;
            }

            bool has = false;
            Card *key;
            foreach(int id, move.card_ids){
                if (Sanguosha->getCard(id)->isKindOf("KeyTrick")){
                    has = true;
                    key = Sanguosha->getCard(id);
                    break;
                }
            }

            if (!has){
                return false;
            }

            CardsMoveStruct new_move;
            new_move.from = move.to;
            new_move.card_ids = move.card_ids;
            new_move.from_pile_name = move.to_pile_name;
            new_move.from_place = move.to_place;
            new_move.reason.m_reason = CardMoveReason::S_REASON_TRANSFER;
            new_move.to = kotori;
            new_move.to_place = Player::PlaceHand;
            room->broadcastSkillInvoke(objectName());
            room->moveCardsAtomic(new_move, true);
            ServerPlayer *target = room->askForPlayerChosen(kotori, room->getOtherPlayers(kotori), objectName());
            room->useCard(CardUseStruct(key, kotori, target));

            //clear old
            foreach(ServerPlayer *p, room->getOtherPlayers(kotori)){
                if (p->getMark("@Jianshi_akarin")){
                    foreach(ServerPlayer *q, room->getOtherPlayers(p)){
                        if (!q->hasSkill(objectName())){
                            room->removeAkarinEffect(p, q);
                        }
                    }
                    p->loseAllMarks("@Jianshi_akarin");
                }
            }

            //add new
            target->gainMark("@Jianshi_akarin");
            foreach(ServerPlayer *p, room->getOtherPlayers(target)){
                if (!p->hasSkill(objectName())){
                    room->akarinPlayer(target, p);
                }
            }

        }
        else if (event == Death){
            DeathStruct death = data.value<DeathStruct>();
            if (death.who->hasSkill(objectName())){
                foreach(ServerPlayer *p, room->getOtherPlayers(death.who)){
                    if (p->getMark("@Jianshi_akarin")){
                        foreach(ServerPlayer *q, room->getOtherPlayers(p)){
                            if (!q->hasSkill(objectName())){
                                room->removeAkarinEffect(p, q);
                            }
                        }
                        p->loseAllMarks("@Jianshi_akarin");
                    }
                }
            }
        }
        return false;
    }

};

class JianshiClear : public DetachEffectSkill
{
public:
    JianshiClear() : DetachEffectSkill("jianshi")
    {
    }

    void onSkillDetached(Room *room, ServerPlayer *player) const
    {
        foreach(ServerPlayer *p, room->getAlivePlayers()){
            if (p->getMark("@Jianshi_akarin")){
                foreach(ServerPlayer *q, room->getOtherPlayers(p)){
                    if (!q->hasSkill(objectName())){
                        room->removeAkarinEffect(p, q);
                    }
                }
                p->loseAllMarks("@Jianshi_akarin");
            }
        }
    }
};

class Qiyue : public TriggerSkill
{
public:
    Qiyue() : TriggerSkill("qiyue")
    {
        events << AskForPeachesDone << BeforeCardsMove;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == AskForPeachesDone){
            DyingStruct dying = data.value<DyingStruct>();
            if (dying.who && dying.who->getHp() <= 0){
                ServerPlayer *kotori = room->findPlayerBySkillName(objectName());
                if (!kotori || !kotori->isAlive() || !kotori->askForSkillInvoke(objectName(), data)){
                    return false;
                }



                room->broadcastSkillInvoke(objectName());
                room->doLightbox("qiyue$", 2000);
                room->setPlayerFlag(kotori, "qiyue_calculate");

                int num = 5 - kotori->getMaxHp();
                foreach(ServerPlayer *p, room->getAlivePlayers()){
                    p->drawCards(num);
                }
                foreach(ServerPlayer *p, room->getAlivePlayers()){
                    for (int i = 0; i < num; i++){
                        if (!p->isAllNude()){
                            room->throwCard(room->askForCardChosen(p, p, "hej", objectName()), p, p);
                        }
                    }
                }
                room->setPlayerFlag(kotori, "-qiyue_calculate");
                if (!kotori->hasFlag("qiyue_return_max")){
                    room->loseMaxHp(kotori);
                }
                else{
                    kotori->setFlags("-qiyue_return_max");
                }
            }
        }
        else if (triggerEvent == BeforeCardsMove){
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (!move.from_places.contains(Player::DrawPile) && !move.from_places.contains(Player::DrawPileBottom)){
                return false;
            }
            ServerPlayer *kotori = room->findPlayerBySkillName(objectName());
            if (kotori && kotori->isAlive() && kotori->hasFlag("qiyue_calculate") && !kotori->hasFlag("qiyue_return_max")){
                if (room->getDrawPile().length() - move.card_ids.length() <= 0){
                    if (kotori->isLord() && room->getAllPlayers(true).length() > 4){
                        room->setPlayerProperty(kotori, "maxhp", QVariant::fromValue(4));
                    }
                    else{
                        room->setPlayerProperty(kotori, "maxhp", QVariant::fromValue(3));
                    }
                    kotori->setFlags("qiyue_return_max");
                }
            }
        }
        return false;
    }

};


class Nangua : public TriggerSkill
{
public:
    Nangua() : TriggerSkill("nangua")
    {
        events << EnterDying << HpRecover;
        frequency = Frequent;
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (event == EnterDying){
            DyingStruct dying = data.value<DyingStruct>();
            if (dying.who->hasSkill(objectName()) && room->askForSkillInvoke(dying.who, objectName(), data)){
                room->broadcastSkillInvoke(objectName());
                dying.who->drawCards(dying.who->getMaxHp());
            }
        }
        else if (event == HpRecover){
            RecoverStruct re = data.value<RecoverStruct>();
            if (re.who && re.who->getHp() < 2 && re.who->hasSkill(objectName()) && room->askForSkillInvoke(re.who, objectName(), data)){
                if (room->askForChoice(re.who, objectName(), "nangua_recover+nangua_turnover", data) == "nangua_recover"){
                    if (re.who->getHp() < 1){
                        room->recover(re.who, RecoverStruct(re.who, NULL, 1 - re.who->getHp()));
                        room->setPlayerProperty(re.who, "hp", 1);
                    }
                }
                else{
                    re.who->turnOver();
                }
            }
        }
        return false;
    }
};

class Jixian : public TriggerSkill
{
public:
    Jixian() : TriggerSkill("jixian")
    {
        events << EventPhaseEnd << AskForPeachesDone;
    }

    void doJixian(Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (player->isNude() || !room->askForSkillInvoke(player, objectName(), data) || !room->askForDiscard(player, objectName(), 1, 1, false, true)){
            return;
        }
        ServerPlayer *p = room->askForPlayerChosen(player, room->getAlivePlayers(), objectName());
        if (p){
            int num = player->getLostHp() + 1;
            if (num > 2){
                room->broadcastSkillInvoke(objectName(), 1);
            }
            else{
                room->broadcastSkillInvoke(objectName(), 2);
            }
            room->damage(DamageStruct(objectName(), player, p, num));
            if (num > 2){
                room->detachSkillFromPlayer(player, objectName());
                room->loseHp(player);  
            }
        }
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (event == EventPhaseEnd && player->hasSkill(objectName()) && player->getPhase() == Player::Finish){
            doJixian(room, player, data);
        }
        else if (event == AskForPeachesDone){
            doJixian(room, player, data);
        }
        return false;
    }
};

class Yandan : public TriggerSkill
{
public:
    Yandan() : TriggerSkill("yandan")
    {
        events << CardsMoveOneTime << Death;
        frequency = Frequent;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime){
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (!move.from_places.contains(Player::PlaceHand) && !move.from_places.contains(Player::PlaceEquip)){
                return false;
            }
            if (move.from->getPhase() != Player::NotActive){
                return false;
            }
            if (move.reason.m_reason != CardMoveReason::S_REASON_DISCARD && move.reason.m_reason != CardMoveReason::S_REASON_DISMANTLE && move.reason.m_reason != CardMoveReason::S_REASON_THROW && move.reason.m_reason != CardMoveReason::S_REASON_RULEDISCARD){
                return false;
            }

            if (move.card_ids.length() == 0){
                return false;
            }

            ServerPlayer *makoto = room->findPlayerBySkillName(objectName());

            if (!makoto || !makoto->isAlive()){
                return false;
            }

            if (makoto->getPile("Yandan").length() >= makoto->getMaxHp()){
                return false;
            }

            if (!makoto->askForSkillInvoke(objectName(), data)){
                return false;
            }
            room->fillAG(move.card_ids, makoto);
            int id = room->askForAG(makoto, move.card_ids, true, objectName());
            room->clearAG(makoto);
            if (id != -1){
                makoto->addToPile("Yandan", id);
            }

        }
        else if (triggerEvent == Death){
            DeathStruct death = data.value<DeathStruct>();
            ServerPlayer *dead = death.who;
            ServerPlayer *makoto = room->findPlayerBySkillName(objectName());
            if (!makoto){
                return false;
            }
            makoto->addMark("yandan_death");
            if (dead->isNude() || makoto == dead){
                return false;
            }
            if (!makoto->askForSkillInvoke(objectName(), data)){
                return false;
            }
            QList<const Card*> cards = dead->getHandcards();
            cards.append(dead->getEquips());
            QList<int> list;
            foreach(const Card* card, cards){
                list.append(card->getId());
            }
            room->fillAG(list, makoto);
            int id = room->askForAG(makoto, list, true, objectName());
            room->clearAG(makoto);
            if (id != -1){
                room->broadcastSkillInvoke(objectName());
                makoto->addToPile("Yandan", id);
            }
        }

        return false;
    }
};

class YandanMaxCards : public MaxCardsSkill
{
public:
    YandanMaxCards() : MaxCardsSkill("#yandan")
    {
    }

    int getExtra(const Player *target) const
    {
        if (target->hasSkill("yandan")){
            int i = target->getPile("Yandan").length() > 0 ? 1 : 0;
            return  i + target->getMark("yandan_death");
        }
        else
            return 0;
    }
};


class YandanClear : public DetachEffectSkill
{
public:
    YandanClear() : DetachEffectSkill("yandan", "Yandan")
    {
    }
};

class Xiwang : public TriggerSkill
{
public:
    Xiwang() : TriggerSkill("xiwang")
    {
        events << EventPhaseStart;
        frequency = Wake;
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (event == EventPhaseStart && player->hasSkill(objectName()) && player->getPhase() == Player::RoundStart){
            if (player->getPile("Yandan").length() > player->getHp() && player->getMark("@waked") == 0){
                room->broadcastSkillInvoke(objectName());
                room->doLightbox("lunpo$", 2000);
                room->loseMaxHp(player);
                player->drawCards(1);
                player->gainMark("@waked");
                room->acquireSkill(player, "lunpo");
            }
        }
        return false;
    }
};


class Lunpo : public TriggerSkill
{
public:
    Lunpo() : TriggerSkill("lunpo")
    {
        events << EventPhaseStart << EventPhaseChanging << Death << CardUsed;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (event == EventPhaseStart){
            if (player->getPhase() == Player::Play && player->hasSkill(objectName())){
                int minHp = 100;
                foreach(ServerPlayer *p, room->getAlivePlayers()){
                    if (p->getHp() < minHp){
                        minHp = p->getHp();
                    }
                }
                if (player->getPile("Yandan").length() < minHp){
                    return false;
                }
                if (!player->askForSkillInvoke("lunpo_inturn", data)){
                    return false;
                }

                QList<int> list = player->getPile("Yandan");

                for (int i = 0; i < minHp; i++){
                    room->fillAG(list, player);
                    int id = room->askForAG(player, list, false, objectName());
                    room->clearAG(player);
                    if (id != -1){
                        list.removeOne(id);
                        room->throwCard(id, player, player);
                    }
                }
                room->broadcastSkillInvoke(objectName(), 1);
                room->doLightbox("lunpo$", 500);
                foreach(ServerPlayer *p, room->getOtherPlayers(player)){
                    p->addMark("lunpo");
                    room->addPlayerMark(p, "@skill_invalidity");

                }
                JsonArray args;
                args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
                room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
            }
        }
        else if (event == CardUsed){
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isKindOf("EquipCard")){
                return false;
            }

            ServerPlayer *makoto = room->findPlayerBySkillName(objectName());
            if (!makoto || !makoto->isAlive()){
                return false;
            }
            if (makoto->getPile("Yandan").length() == 0){
                return false;
            }
            QList<int> list;
            foreach(int id, makoto->getPile("Yandan")){
                if (Sanguosha->getCard(id)->getSuit() == use.card->getSuit()){
                    list.append(id);
                }
            }

            if (list.length() == 0 || !makoto->askForSkillInvoke(objectName(), data)){
                return false;
            }

            room->fillAG(list, makoto);
            int id = room->askForAG(makoto, list, true, objectName());
            room->clearAG(makoto);
            if (id != -1){
                room->throwCard(id, makoto, makoto);
                room->broadcastSkillInvoke(objectName(), 2);
                room->doLightbox("lunpo$", 300);
                if (use.card->isKindOf("DelayedTrick")){
                    room->throwCard(use.card->getId(), makoto, makoto);
                }
                return true;
            }

        }
        else if (event == EventPhaseChanging){
            QList<ServerPlayer *> players = room->getAllPlayers();
            foreach(ServerPlayer *player, players) {
                if (player->getMark("lunpo") == 0) continue;
                player->removeMark("lunpo");
                room->removePlayerMark(player, "@skill_invalidity");
            }
            JsonArray args;
            args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
            room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
        }
        else if (event == Death){
            DeathStruct death = data.value<DeathStruct>();
            if (death.who->hasSkill(objectName())){
                QList<ServerPlayer *> players = room->getAllPlayers();
                foreach(ServerPlayer *player, players) {
                    if (player->getMark("lunpo") == 0) continue;
                    player->removeMark("lunpo");
                    room->removePlayerMark(player, "@skill_invalidity");
                }
                JsonArray args;
                args << QSanProtocol::S_GAME_EVENT_UPDATE_SKILL;
                room->doBroadcastNotify(QSanProtocol::S_COMMAND_LOG_EVENT, args);
            }
        }
        return false;
    }
};

class LunpoInvalidity : public InvaliditySkill
{
public:
    LunpoInvalidity() : InvaliditySkill("#lunpo-inv")
    {
    }

    bool isSkillValid(const Player *player, const Skill *skill) const
    {
        return player->getMark("lunpo") == 0 || skill->isAttachedLordSkill();
    }
};

class Xinyang : public TriggerSkill
{
public:
    Xinyang() : TriggerSkill("xinyang")
    {
        events << CardShown << StartJudge;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (event == CardShown){
            ServerPlayer * sanae = room->findPlayerBySkillName(objectName());
            if (!sanae || sanae->isDead()){
                return false;
            }
            if (!room->askForSkillInvoke(sanae, objectName(), data)){
                return false;
            }
            sanae->addToPile("xinyang", room->getDrawPile().first());
        }
        else if (event == StartJudge){
            if (player && player->isAlive()){
                ServerPlayer * sanae = room->findPlayerBySkillName(objectName());
                if (sanae && sanae->isAlive() && sanae->hasSkill(objectName()) && sanae->getPile("xinyang").length() > 0 && room->askForSkillInvoke(sanae, "xinyang_judge", data)){
                    room->fillAG(sanae->getPile("xinyang"), sanae);
                    int id = room->askForAG(sanae, sanae->getPile("xinyang"), true, objectName());
                    room->clearAG(sanae);
                    if (id != -1){
                        room->moveCardTo(Sanguosha->getCard(id), sanae, NULL, Player::DrawPile, CardMoveReason(CardMoveReason::S_REASON_PUT, sanae->objectName()), true);
                    }
                }
            }
        }
        return false;
    }
};

class XinyangClear : public DetachEffectSkill
{
public:
    XinyangClear() : DetachEffectSkill("xinyang", "xinyang")
    {
    }
};


FengzhuDialog::FengzhuDialog() : GuhuoDialog("fengzhu", true, false)
{

}

FengzhuDialog *FengzhuDialog::getInstance()
{
    static FengzhuDialog *instance;
    if (instance == NULL || instance->objectName() != "fengzhu")
        instance = new FengzhuDialog;

    return instance;
}

bool FengzhuDialog::isButtonEnabled(const QString &button_name) const
{
    if (Self->hasFlag("fengzhu_used"))
        return false;

    return GuhuoDialog::isButtonEnabled(button_name);
}

FengzhuCard::FengzhuCard()
{
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool FengzhuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
    }

    const Card *_card = Self->tag.value("fengzhu").value<const Card *>();
    if (_card == NULL)
        return false;

    Card *card = Sanguosha->cloneCard(_card->objectName(), Card::NoSuit, 0);
    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetFilter(targets, to_select, Self) && !Self->isProhibited(to_select, card, targets);
}

bool FengzhuCard::targetFixed() const
{
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetFixed();
    }

    const Card *_card = Self->tag.value("fengzhu").value<const Card *>();
    if (_card == NULL)
        return false;

    Card *card = Sanguosha->cloneCard(_card->objectName(), Card::NoSuit, 0);
    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetFixed();
}

bool FengzhuCard::targetsFeasible(const QList<const Player *> &targets, const Player *Self) const
{
    if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        const Card *card = NULL;
        if (!user_string.isEmpty())
            card = Sanguosha->cloneCard(user_string.split("+").first());
        return card && card->targetsFeasible(targets, Self);
    }

    const Card *_card = Self->tag.value("fengzhu").value<const Card *>();
    if (_card == NULL)
        return false;

    Card *card = Sanguosha->cloneCard(_card->objectName(), Card::NoSuit, 0);
    card->setCanRecast(false);
    card->deleteLater();
    return card && card->targetsFeasible(targets, Self);
}

const Card *FengzhuCard::validate(CardUseStruct &card_use) const
{
    ServerPlayer *sanae = card_use.from;
    Room *room = sanae->getRoom();

    QString to_guhuo = user_string;
    if (user_string == "slash" && Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE) {
        QStringList guhuo_list;
        guhuo_list << "slash";
        if (!Config.BanPackages.contains("maneuvering"))
            guhuo_list = QStringList() << "normal_slash" << "thunder_slash" << "fire_slash";
        to_guhuo = room->askForChoice(sanae, "fengzhu_slash", guhuo_list.join("+"));
    }

    //room->moveCardTo(this, NULL, Player::DrawPile, true);

    QString user_str;
    if (to_guhuo == "normal_slash")
        user_str = "slash";
    else
        user_str = to_guhuo;

    Card *c = Sanguosha->cloneCard(user_str, Card::NoSuit, 0);

    QString classname;
    if (c->isKindOf("Slash"))
        classname = "Slash";
    else
        classname = c->getClassName();

    room->setPlayerFlag(sanae, "fengzhu_used");

    if (sanae && sanae->isAlive() && sanae->hasSkill("fengzhu")){
        const Card* card = room->askForCardShow(sanae, sanae, "fengzhu");
        if (card){
            room->showCard(sanae, card->getEffectiveId());
            JudgeStruct judge;
            judge.reason = "fengzhu";
            judge.who = sanae;
            judge.pattern = ".|" + card->getSuitString();
            room->judge(judge);
            if (judge.isGood()){
                c->setSkillName("fengzhu");
                c->deleteLater();
                return c;
            }
            else{
                room->obtainCard(sanae, judge.card->getEffectiveId());
            }
        }
    }

    return NULL;
}

const Card *FengzhuCard::validateInResponse(ServerPlayer *sanae) const
{
    Room *room = sanae->getRoom();

    QString to_guhuo = user_string;
    if (user_string == "peach+analeptic") {
        bool can_use_peach = !sanae->hasFlag("fengzhu_used");
        bool can_use_analeptic = !sanae->hasFlag("fengzhu_used");
        QStringList guhuo_list;
        if (can_use_peach)
            guhuo_list << "peach";
        if (can_use_analeptic && !Config.BanPackages.contains("maneuvering"))
            guhuo_list << "analeptic";
        to_guhuo = room->askForChoice(sanae, "fengzhu_saveself", guhuo_list.join("+"));
    }
    else if (user_string == "slash") {
        QStringList guhuo_list;
        guhuo_list << "slash";
        if (!Config.BanPackages.contains("maneuvering"))
            guhuo_list = QStringList() << "normal_slash" << "thunder_slash" << "fire_slash";
        to_guhuo = room->askForChoice(sanae, "fengzhu_slash", guhuo_list.join("+"));
    }
    else
        to_guhuo = user_string;

    //room->moveCardTo(this, NULL, Player::DrawPile, true);

    QString user_str;
    if (to_guhuo == "normal_slash")
        user_str = "slash";
    else
        user_str = to_guhuo;

    Card *c = Sanguosha->cloneCard(user_str, Card::NoSuit, 0);

    QString classname;
    if (c->isKindOf("Slash"))
        classname = "Slash";
    else
        classname = c->getClassName();

    room->setPlayerFlag(sanae, "fengzhu_used");

    if (sanae && sanae->isAlive() && sanae->hasSkill("fengzhu") && !sanae->isKongcheng()){
        const Card* card = room->askForCardShow(sanae, sanae, "fengzhu");
        if (card){
            JudgeStruct judge;
            judge.reason = "fengzhu";
            judge.who = sanae;
            judge.pattern = ".|" + card->getSuitString();
            room->judge(judge);
            if (judge.isGood()){
                c->setSkillName("fengzhu");
                c->deleteLater();
                return c;
            }
            else{
                room->obtainCard(sanae, judge.card->getEffectiveId());
            }
        }
    }

    return NULL;

}

class FengzhuVS : public ZeroCardViewAsSkill
{
public:
    FengzhuVS() : ZeroCardViewAsSkill("fengzhu")
    {
    }

    const Card *viewAs() const
    {
        QString pattern;

        if (Self->getHandcardNum() == 0){
            return NULL;
        }

        if (Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_PLAY) {
            const Card *c = Self->tag["fengzhu"].value<const Card *>();
            if (c == NULL || Self->hasFlag("fengzhu_used"))
                return NULL;

            pattern = c->objectName();
        }
        else {
            pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
            if (pattern == "peach+analeptic" && Self->getMark("Global_PreventPeach") > 0)
                pattern = "analeptic";

            // check if it can use
            bool can_use = false;
            QStringList p = pattern.split("+");
            foreach(const QString &x, p) {
                const Card *c = Sanguosha->cloneCard(x);
                QString us = c->getClassName();
                if (c->isKindOf("Slash"))
                    us = "Slash";

                if (!Self->hasFlag("fengzhu_used"))
                    can_use = true;

                delete c;
                if (can_use)
                    break;
            }

            if (!can_use)
                return NULL;
        }

        FengzhuCard *fz = new FengzhuCard;
        fz->setUserString(pattern);

        return fz;

    }

    bool isEnabledAtPlay(const Player *player) const
    {
        if (player->isKongcheng()){
            return false;
        }

        if (player->hasFlag("fengzhu_used")){
            return false;
        }

        QList<const Player *> sib = player->getAliveSiblings();
        if (player->isAlive())
            sib << player;

        bool noround = true;

        foreach(const Player *p, sib) {
            if (p->getPhase() != Player::NotActive) {
                noround = false;
                break;
            }
        }

        return true; // for DIY!!!!!!!
    }

    bool isEnabledAtResponse(const Player *player, const QString &pattern) const
    {
        QList<const Player *> sib = player->getAliveSiblings();
        if (player->isAlive())
            sib << player;

        bool noround = true;

        foreach(const Player *p, sib) {
            if (p->getPhase() != Player::NotActive) {
                noround = false;
                break;
            }
        }

        if (noround)
            return false;

        if (Sanguosha->currentRoomState()->getCurrentCardUseReason() != CardUseStruct::CARD_USE_REASON_RESPONSE_USE)
            return false;

#define FENGZHU_CAN_USE(x) (!player->hasFlag("fengzhu_used"))

        if (pattern == "slash")
            return FENGZHU_CAN_USE(Slash);
        else if (pattern == "peach")
            return FENGZHU_CAN_USE(Peach) && player->getMark("Global_PreventPeach") == 0;
        else if (pattern.contains("analeptic"))
            return FENGZHU_CAN_USE(Peach) || FENGZHU_CAN_USE(Analeptic);
        else if (pattern == "jink")
            return FENGZHU_CAN_USE(Jink);

#undef FENGZHU_CAN_USE

        return false;
    }
};

class Fengzhu : public TriggerSkill
{
public:
    Fengzhu() : TriggerSkill("fengzhu")
    {
        view_as_skill = new FengzhuVS;
        events << EventPhaseChanging;
    }

    QDialog *getDialog() const
    {
        return FengzhuDialog::getInstance();
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to != Player::NotActive)
            return false;

        foreach(ServerPlayer *p, room->getAlivePlayers()) {
            if (p->hasFlag("fengzhu_used"))
                room->setPlayerFlag(p, "-fengzhu_used");
        }

        return false;
    }
};


/*
class Fengzhu : public TriggerSkill
{
public:
    Fengzhu() : TriggerSkill("fengzhu")
    {
        events << CardsMoveOneTime;
        frequency = Compulsory;
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (event == CardsMoveOneTime){
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.to_place != Player::DiscardPile){
                return false;
            }

            if (move.card_ids.length() == 0){
                return false;
            }

            if (move.reason.m_reason != CardMoveReason::S_REASON_USE && move.reason.m_reason != CardMoveReason::S_REASON_LETUSE){
                return false;
            }

            ServerPlayer *sanae = room->findPlayerBySkillName(objectName());
            if (!sanae || sanae->isDead()){
                return false;
            }
            QList<int> all;
            all.append(room->getDrawPile().at(room->getDrawPile().length() - 2));
            all.append(room->getDrawPile().at(room->getDrawPile().length() - 1));
            foreach(int id, all){
                int r = rand() % 3;
                if (r == 0){
                    room->moveCardTo(Sanguosha->getCard(id), sanae, Player::PlaceHand, CardMoveReason(6, sanae->objectName()));
                }
                else if (r == 1){
                    room->moveCardTo(Sanguosha->getCard(id), NULL, Player::DrawPile);
                }
                else{
                    room->moveCardTo(Sanguosha->getCard(id), NULL, Player::DrawPileBottom);
                }
            }
        }
        return false;
    }
};
*/

class Zuzhou : public TriggerSkill
{
public:
    Zuzhou() : TriggerSkill("zuzhou")
    {
        frequency = Compulsory;
        events << TargetConfirmed << EventPhaseEnd << Death;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == TargetConfirmed){
            CardUseStruct use = data.value<CardUseStruct>();
            foreach(ServerPlayer *p, use.to){
                if (p->hasSkill(objectName()) && p == player && use.from && p != use.from){
                    if (use.from->getMark("@zuzhou") == 0){
                        room->broadcastSkillInvoke(objectName());
                    }
                    if (p->getLostHp() == 0){
                        use.from->gainMark("@zuzhou", 1);
                    }
                    else{
                        use.from->gainMark("@zuzhou", p->getLostHp());
                    }
                }
            }
        }
        else if (triggerEvent == EventPhaseEnd){
            if (player->getPhase() != Player::Discard){
                return false;
            }
            bool will_turen = false;
            if (player->getMaxCards() > 0)
                will_turen = true;

            player->loseAllMarks("@zuzhou");

            if (will_turen){
                return false;
            }
            ServerPlayer *f = room->findPlayerBySkillName(objectName());
            if (f && f->isAlive()){
                f->drawCards(1);
            }
        }
        else if (triggerEvent == Death){
            DeathStruct death = data.value<DeathStruct>();
            if (death.who->hasSkill(objectName())){
                foreach(ServerPlayer *p, room->getAlivePlayers()){
                    p->loseAllMarks("@zuzhou");
                }
            }
        }

        return false;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }
};

class ZuzhouClear : public DetachEffectSkill
{
public:
    ZuzhouClear() : DetachEffectSkill("zuzhou")
    {
    }

    void onSkillDetached(Room *room, ServerPlayer *player) const
    {
        foreach(ServerPlayer *p, room->getAlivePlayers()){
            p->loseAllMarks("@zuzhou");
        }
    }
};

class ZuzhouMaxCards : public MaxCardsSkill
{
public:
    ZuzhouMaxCards() : MaxCardsSkill("#zuzhou")
    {
    }

    int getExtra(const Player *target) const
    {
        if (target->getMark("@zuzhou") > 0){
            return  -target->getMark("@zuzhou");
        }
        else
            return 0;
    }
};


JiguanCard::JiguanCard()
{
    target_fixed = true;
}

void JiguanCard::use(Room *room, ServerPlayer *fear, QList<ServerPlayer *> &) const
{
    fear->drawCards(1);
    QList<int> ids;
    foreach(const Card* card, fear->getHandcards()){
        if (card->isBlack()){
            ids.append(card->getId());
        }
    }
    foreach(const Card* card, fear->getEquips()){
        if (card->isBlack()){
            ids.append(card->getId());
        }
    }

    for (int i = 0; i < 2; i++){
        if (ids.length() > 0){
            if (room->askForChoice(fear, "jiguan", "jiguan_put+jiguan_pass") == "jiguan_put"){
                room->fillAG(ids, fear);
                int id = room->askForAG(fear, ids, true, objectName());
                room->clearAG(fear);
                if (id != -1){
                    ids.removeOne(id);
                    fear->addToPile("jiguan", id, false);
                }
                else{
                    break;
                }
            }
            else{
                break;
            }

        }
    }

}

class JiguanVS : public ZeroCardViewAsSkill
{
public:
    JiguanVS() : ZeroCardViewAsSkill("jiguan")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("JiguanCard");
    }

    const Card *viewAs() const
    {
        return new JiguanCard();
    }
};

class Jiguan : public TriggerSkill
{
public:
    Jiguan() : TriggerSkill("jiguan")
    {
        view_as_skill = new JiguanVS;
        events << CardUsed;
    }

    bool trigger(TriggerEvent, Room *room, ServerPlayer *, QVariant &data) const
    {
        CardUseStruct use = data.value<CardUseStruct>();
        ServerPlayer *fear = room->findPlayerBySkillName(objectName());
        if (!fear || fear->isDead() || fear->getPile("jiguan").length() == 0){
            return false;
        }
        if (use.from->isDead()){
            return false;
        }
        QList<int> ava;
        foreach(int id, fear->getPile("jiguan")){
            if (Sanguosha->getCard(id)->getNumber() == use.card->getNumber()){
                ava.append(id);
            }
        }

        if (ava.length() == 0){
            return false;
        }

        if (!room->askForSkillInvoke(fear, objectName(), data)){
            return false;
        }

        room->fillAG(ava, fear);
        int tid = room->askForAG(fear, ava, true, objectName());
        room->clearAG(fear);
        if (tid == -1){
            return false;
        }
        room->showCard(fear, tid);
        room->broadcastSkillInvoke(objectName());
        room->doAnimate(QSanProtocol::S_ANIMATE_LIGHTBOX, "lani=skills/jiguan", QString("%1:%2").arg(1000).arg(0));
        if (use.from == fear){
            room->loseHp(room->askForPlayerChosen(fear, room->getAlivePlayers(), objectName()));
        }
        else{
            room->throwCard(tid, fear, fear);
            room->loseHp(use.from);
        }


        return false;
    }
    bool triggerable(const ServerPlayer *target) const
    {
        return target;
    }

};

class JiguanClear : public DetachEffectSkill
{
public:
    JiguanClear() : DetachEffectSkill("jiguan", "jiguan")
    {
    }
};

//misaka mikoto
PaojiCard::PaojiCard()
{
    mute = true;
}

bool PaojiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty()) return false;
    return true;
}

void PaojiCard::use(Room *room, ServerPlayer *mikoto, QList<ServerPlayer *> &targets) const
{
   ServerPlayer *target = targets.at(0);
   room->broadcastSkillInvoke("paoji");
   Card *sub = Sanguosha->getCard(this->getSubcards().at(0));
   Card *card = Sanguosha->cloneCard("thunder_slash",sub->getSuit(), sub->getNumber());
   card->addSubcard(sub);
   room->useCard(CardUseStruct(card, mikoto, target));
}

class Paojivs : public ViewAsSkill
{
public:
    Paojivs() :ViewAsSkill("paoji")
    {
    }

    bool isEnabledAtPlay(const Player *player) const
    {
        return !player->hasUsed("PaojiCard")&&player->hasSkill("paoji");
    }

    bool viewFilter(const QList<const Card *> &selected, const Card *to_select) const
    {
        return selected.length() == 0 && !to_select->isEquipped();
    }

    const Card *viewAs(const QList<const Card *> &cards) const
    {
        if (cards.isEmpty())
            return NULL;
        PaojiCard *pj = new PaojiCard();
        pj->addSubcards(cards);
        return pj;
    }
};

class Paoji : public TriggerSkill
{
public:
    Paoji() : TriggerSkill("paoji")
    {
        events << GameStart << CardUsed << DamageCaused;
        global=true;
        view_as_skill=new Paojivs;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        /*PhaseChangeStruct change = data.value<PhaseChangeStruct>();
        if (change.to == Player::Draw && room->askForSkillInvoke(mikoto, objectName(), data)){
            mikoto->skip(Player::Draw);
            QStringList stringlist;
            for (int i = 1; i <= mikoto->getMaxHp(); i++){
                stringlist.append(QString::number(i));
            }
            ServerPlayer *target = room->askForPlayerChosen(mikoto, room->getAlivePlayers(), objectName());
            if (!target){
                return false;
            }
            int num = room->askForChoice(mikoto, objectName(), stringlist.join("+"), data).toInt();

            QList<int> card_ids;
            QList<Card::Color> colors;
            for (int i = 0; i < num; i++){
                JudgeStruct judge;
                judge.reason = objectName();
                judge.play_animation = true;
                judge.who = target;
                room->judge(judge);
                card_ids.append(judge.card->getEffectiveId());
                if (!colors.contains(judge.card->getColor())){
                    colors.append(judge.card->getColor());
                }
                if (i == 0){
                    room->setTag("paoji_first_color", QVariant(judge.card->isRed()));
                }
            }
            if (colors.length() == 1){
                room->damage(DamageStruct(objectName(), mikoto, target, 1, DamageStruct::Thunder));
                room->broadcastSkillInvoke(objectName(), 2 + rand() % 2);
            }
            else{
                room->broadcastSkillInvoke(objectName(), 1);
            }

            DummyCard *dummy = new DummyCard(card_ids);
            room->obtainCard(mikoto, dummy);
        }*/

        if (triggerEvent==GameStart){
            if (player->hasSkill(objectName())) {
                player->gainMark("@ying",4);
            }
        }
        else if(triggerEvent==CardUsed){
            if (!player->hasSkill(objectName())){
                return false;
            }
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->objectName()!="thunder_slash"||!player->askForSkillInvoke(objectName(),data)){
                return false;
            }
            room->broadcastSkillInvoke(objectName());
            if (player->getMark("@ying")>0){
                player->loseMark("@ying");
                JudgeStruct judge;
                judge.pattern = "club|spade";
                judge.good = true;
                judge.reason = objectName();
                judge.who = player;
                room->judge(judge);
                if (judge.card->isBlack()){
                    use.card->setFlags(objectName());
                }
            }
            else if(player->getMark("@ying")==0){
                QStringList stringlist;
                for (int i = 1; i <= room->getAlivePlayers().length(); i++){
                    stringlist.append(QString::number(i));
                }
                int num = room->askForChoice(player, "paoji_addtargets", stringlist.join("+"), data).toInt();
                for (int i = 1; i <= num; i++){
                    stringlist.append(QString::number(i));
                    ServerPlayer *target = room->askForPlayerChosen(player, room->getAlivePlayers(), objectName());
                    if (!use.to.contains(target)){
                        use.to.append(target);
                    }
                }
                data.setValue(use);
                use.card->setFlags(objectName());
                room->detachSkillFromPlayer(player,objectName());
            }
        }
        else if (triggerEvent==DamageCaused){
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.card&&damage.card->hasFlag(objectName())){
                    damage.damage=damage.damage+1;
                    data.setValue(damage);
                    damage.card->clearFlags();
            }
        }
        return false;
    }
};

class Dianci : public TriggerSkill
{
public:
    Dianci() : TriggerSkill("dianci")
    {
        events << EventPhaseStart << EventPhaseEnd;
        global=true;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        /*CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        if (move.from && move.from->hasSkill(objectName()) && (!move.to || move.to != move.from) && move.from->getPhase() == Player::NotActive){
            ServerPlayer *mikoto = room->findPlayerBySkillName(objectName());
            if (!mikoto || !mikoto->askForSkillInvoke(objectName(), data)){
                return false;
            }
            foreach(int id, move.card_ids){
                ServerPlayer *target = room->askForPlayerChosen(mikoto, room->getAlivePlayers(), objectName());
                if (!target){
                    return false;
                }
                room->setPlayerProperty(target, "chained", QVariant(true));

            }
        }*/
        ServerPlayer *sp=room->findPlayerBySkillName(objectName());
        if (!sp){
            return false;
        }
        if (triggerEvent==EventPhaseStart){
            if (sp->distanceTo(player)>1||player->isKongcheng()||player->getPhase()!=Player::RoundStart||!sp->askForSkillInvoke(objectName(),data)){
                return false;
            }
            int id=room->askForCardChosen(sp,player,"h",objectName());
            room->showCard(player, id, sp);
            Card *c=Sanguosha->getCard(id);
            if (c->isBlack()){
                 QString choice=room->askForChoice(sp,objectName(),"dianci_obtain+dianci_kill+dianci_chain+dianci_give");
                 room->broadcastSkillInvoke(objectName());
                 if (choice=="dianci_obtain"){
                     room->obtainCard(sp,c);
                     room->setPlayerFlag(player, sp->objectName()+"dianci_pro");
                 }
                 else if (choice=="dianci_kill"){
                     Slash *slash = new Slash(c->getSuit(), c->getNumber());
                     slash->addSubcard(c);
                     ServerPlayer *target = room->askForPlayerChosen(sp, room->getAlivePlayers(), objectName());
                     if (!target){
                         return false;
                     }
                     room->useCard(CardUseStruct(slash, sp, target));
                 }
                 else if (choice=="dianci_chain"){
                     QList<int> ids;
                     ids.append(id);
                     CardsMoveStruct move(ids, NULL, Player::DrawPile,
                         CardMoveReason(CardMoveReason::S_REASON_PUT, sp->objectName(), objectName(), QString()));
                     room->moveCardsAtomic(move,false);
                     for (int i=0;i<2;i++){
                         ServerPlayer *target = room->askForPlayerChosen(sp, room->getAlivePlayers(), objectName());
                         if (!target){
                             return false;
                         }
                         room->setPlayerProperty(target, "chained", QVariant(true));
                     }
                 }
                 else {
                     ServerPlayer *target=room->askForPlayerChosen(sp,room->getOtherPlayers(player),objectName());
                     room->obtainCard(target,c);
                     QString type="";
                     if (c->getSuit()==Card::Spade){
                         type=".|spade|.|hand";
                     }
                     else if (c->getSuit()==Card::Heart){
                         type=".|heart|.|hand";
                     }
                     else if (c->getSuit()==Card::Club){
                         type=".|club|.|hand";
                     }
                     else{
                         type=".|diamond|.|hand";
                     }
                     room->setPlayerCardLimitation(target,"discard,use,response",type,false);
                     room->setTag(target->objectName()+"dianci",QVariant(type));
                     if (sp->objectName()==player->objectName()){
                         room->setPlayerMark(sp,"thisturn",1);
                     }
                 }
            }
        }
        else if (triggerEvent==EventPhaseEnd){
            if (player->getPhase()==Player::Finish&&player->hasSkill(objectName())){
                if (player->getMark("thisturn")>0){
                    room->setPlayerMark(player,"thisturn",0);
                    return false;
                }
                foreach (ServerPlayer *p,room->getAlivePlayers()){
                    QString s=room->getTag(p->objectName()+"dianci").toString();
                    if (s!=""){
                        room->removePlayerCardLimitation(p,"discard,use,response",s);
                    }
                }
            }
        }
        return false;
    }
};

class DianciProhibit : public ProhibitSkill
{
public:
    DianciProhibit() : ProhibitSkill("#dianci")
    {
    }

    bool isProhibited(const Player *from, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        if (from->hasFlag(to->objectName()+"dianci_pro")){
            return true;
        }
        return false;
    }
};


class Shuji : public TriggerSkill
{
public:
    Shuji() : TriggerSkill("shuji")
    {
        events << CardsMoveOneTime << EventPhaseStart << EventPhaseEnd;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == CardsMoveOneTime){
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.reason.m_reason != CardMoveReason::S_REASON_USE && move.reason.m_reason != CardMoveReason::S_REASON_LETUSE){
                return false;
            }

            if (move.card_ids.length() == 0 || move.to_place != Player::DiscardPile){
                return false;
            }

            ServerPlayer *dalian = room->findPlayerBySkillName(objectName());

            if (!dalian || !dalian->isAlive() || dalian->isKongcheng()){
                return false;
            }
            foreach(int card_id, move.card_ids){
                Card *card = Sanguosha->getCard(card_id);
                if (card->isKindOf("TrickCard")){

                    QList<int> list = dalian->getPile("huanshu");
                    if (list.length() > 8){
                        return false;
                    }
                    bool has_same = false;
                    foreach(int id, list){
                        if (Sanguosha->getCard(id)->getClassName() == card->getClassName()){
                            has_same = true;
                            break;
                        }
                    }

                    if (has_same){
                        continue;
                    }

                    room->setTag("shuji-card", QVariant(card_id));
                    if (room->askForDiscard(dalian, objectName(), 1, 1, true, true, "@shuji-discard")){
                        if (dalian->getGeneral2Name() == "Hugh"){
                            room->broadcastSkillInvoke(objectName(), 3);
                        }
                        else{
                            room->broadcastSkillInvoke(objectName(), rand() % 2 + 1);
                        }

                        dalian->addToPile("huanshu", card_id);
                    }
                    room->removeTag("shuji-card");
                }
            }
        }
        else if (triggerEvent == EventPhaseStart){
            if (player->hasSkill(objectName()) && player->getPhase() == Player::Discard){
                QString _type = "TrickCard|.|.|hand"; // Handcards only
                room->setPlayerCardLimitation(player, "discard", _type, true);
            }
        }
        else if (triggerEvent == EventPhaseEnd){
            if (player->hasSkill(objectName()) && player->getPhase() == Player::Discard){
                QString _type = "TrickCard|.|.|hand"; // Handcards only
                room->removePlayerCardLimitation(player, "discard", _type);
            }
        }
        return false;
    }
};

class ShujiMaxCards : public MaxCardsSkill
{
public:
    ShujiMaxCards() : MaxCardsSkill("#shuji")
    {
    }

    int getExtra(const Player *target) const
    {
        if (target->hasSkill("shuji")){
            int num = 0;
            foreach (const Card* card, target->getHandcards()){
                num += card->isKindOf("TrickCard") ? 1 : 0;
            }
            return  num;
        }
        else
            return 0;
    }
};

class Jicheng : public TriggerSkill
{
public:
    Jicheng() : TriggerSkill("jicheng")
    {
        events << EventPhaseStart;
        frequency = Wake;
    }

    bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (event == EventPhaseStart && player->hasSkill(objectName()) && player->getPhase() == Player::RoundStart){
            int minHp = 100;
            int minHand = 100;
            foreach(ServerPlayer *p, room->getOtherPlayers(player)){
                if (p->getHp() < minHp){
                    minHp = p->getHp();
                }
                if (p->getHandcardNum() < minHand){
                    minHand = p->getHandcardNum();
                }
            }
            if ((player->getHandcardNum() < minHand || player->getHp() < minHp) && player->getMark("@waked") == 0){
                room->broadcastSkillInvoke(objectName());
                room->doLightbox("jicheng$", 3000);
                room->recover(player, RecoverStruct(player, 0, player->getLostHp()));
                player->drawCards(2);
                player->gainMark("@waked");
                room->changeHero(player, "Hugh", false, false, true);
            }
        }
        return false;
    }
};

/*
class ShoushiProhibit : public ProhibitSkill
{
public:
    ShoushiProhibit() : ProhibitSkill("#shoushi")
    {
    }

    bool isProhibited(const Player *, const Player *to, const Card *card, const QList<const Player *> &) const
    {
        if (!to->hasSkill(this)){
            return false;
        }
        foreach(int card_id, to->getPile("huanshu")){
            if (Sanguosha->getCard(card_id)->getClassName() == card->getClassName()){
                return true;
            }
        }
        return false;
    }
};*/

class Shoushi : public TriggerSkill
{
public:
    Shoushi() : TriggerSkill("shoushi")
    {
        events << PreCardUsed << TrickCardCanceling << TargetConfirmed;
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const
    {
        if (triggerEvent == TrickCardCanceling){
            CardEffectStruct effect = data.value<CardEffectStruct>();
            if (effect.from && effect.from->hasSkill(objectName())){
                if (!effect.card || !effect.card->isNDTrick()){
                    return false;
                }
                int num = 0;
                ServerPlayer *jianyong = effect.from;
                foreach(int card_id, jianyong->getPile("huanshu")){
                    num += Sanguosha->getCard(card_id)->getSuit() == effect.card->getSuit() ? 1 : 0;
                }
                if (num == 0){
                    return false;
                }

                if (!jianyong->hasFlag("Shoushi_sound_used")){
                    room->broadcastSkillInvoke(objectName());
                    jianyong->setFlags("Shoushi_sound_used");
                }

                return true;
            }

        }
        if (triggerEvent == TargetConfirmed && TriggerSkill::triggerable(player)) {
            CardUseStruct use = data.value<CardUseStruct>();

            if (use.to.contains(player) && use.from != player) {
                if (use.card && use.card->isNDTrick()) {
                    bool can_trigger = false;
                    foreach(int card_id, player->getPile("huanshu")){
                        if (Sanguosha->getCard(card_id)->getClassName() == use.card->getClassName()){
                            can_trigger = true;
                            break;
                        }
                    }

                    if (can_trigger && room->askForSkillInvoke(player, objectName(), data)) {
                        room->broadcastSkillInvoke(objectName());
                        use.nullified_list << player->objectName();
                        data = QVariant::fromValue(use);
                    }
                }
            }
        }
        else if (triggerEvent == PreCardUsed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.card->isNDTrick() && use.from->hasSkill(objectName())) {
                ServerPlayer *jianyong = use.from;
                int num = 0;
                foreach(int card_id, jianyong->getPile("huanshu")){
                    num += Sanguosha->getCard(card_id)->getSuit() == use.card->getSuit() ? 1 : 0;
                }


                //1
                // cannot wu xie ke ji


                if (num < 2){
                    return false;
                }

                jianyong->drawCards(1);

                // 2

                if (num < 3){
                    return false;
                }
                if (use.card->isKindOf("Collateral")){
                    return false;
                }
                QList<ServerPlayer *> available_targets;
                if (!use.card->isKindOf("AOE") && !use.card->isKindOf("GlobalEffect")) {
                    room->setPlayerFlag(jianyong, "ShoushiExtraTarget");
                    foreach(ServerPlayer *p, room->getAlivePlayers()) {
                        if (use.to.contains(p) || room->isProhibited(jianyong, p, use.card)) continue;
                        if (use.card->targetFixed()) {
                            if (!use.card->isKindOf("Peach") || p->isWounded())
                                available_targets << p;
                        }
                        else {
                            if (use.card->targetFilter(QList<const Player *>(), p, jianyong))
                                available_targets << p;
                        }
                    }
                    room->setPlayerFlag(jianyong, "-ShoushiExtraTarget");
                }
                QStringList choices;
                choices << "cancel";
                if (use.to.length() > 1) choices.prepend("remove");
                if (!available_targets.isEmpty()) choices.prepend("add");
                if (choices.length() == 1) return false;

                QString choice = room->askForChoice(jianyong, "shoushi", choices.join("+"), data);
                if (choice == "cancel")
                    return false;
                else if (choice == "add") {
                    ServerPlayer *extra = NULL;
                    extra = room->askForPlayerChosen(jianyong, available_targets, "shoushi", "@shoushi-add:::" + use.card->objectName());
                    use.to.append(extra);
                    room->sortByActionOrder(use.to);
                }
                else {
                    ServerPlayer *removed = room->askForPlayerChosen(jianyong, use.to, "shoushi", "@shoushi-remove:::" + use.card->objectName());
                    use.to.removeOne(removed);
                }
            }
            data = QVariant::fromValue(use);

        }

        return false;
    }
};


class Kaiqi : public TriggerSkill
{
public:
    Kaiqi() : TriggerSkill("kaiqi")
    {
        events << EventPhaseStart;
    }

    bool trigger(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &) const
    {
        if (triggerEvent == EventPhaseStart){
            QList<ServerPlayer *> left = room->getAlivePlayers();
            if (player->hasSkill(objectName()) && player->getPhase() == Player::Play){
                int i = 0;
                while (player->getPile("huanshu").length() > 0 && left.length() > 0){
                    ServerPlayer *target = room->askForPlayerChosen(player, left, objectName(), "@shuji-prompt", true);
                    if (!target){
                        return false;
                    }
                    if (i == 0){
                        room->broadcastSkillInvoke(objectName());
                        room->doLightbox("kaiqi$", 800);
                    }
                    i++;

                    left.removeOne(target);
                    QList<int> card_ids = player->getPile("huanshu");
                    room->fillAG(card_ids, player);
                    int id = room->askForAG(player, card_ids, false, objectName());
                    room->clearAG(player);
                    if (id == -1){
                        return false;
                    }
                    room->obtainCard(target, id);
                }
            }
        }
        return false;
    }
};

class GeneralSkillInvalidity : public InvaliditySkill
{
public:
    GeneralSkillInvalidity() : InvaliditySkill("#general-skill-invalidity")
    {
    }

    bool isSkillValid(const Player *player, const Skill *skill) const
    {
        return (player->getMark("@skill_invalidity") == 0 || skill->getFrequency(player) == Skill::Compulsory) && player->getMark("@all_skill_invalidity") == 0;
    }
};


InovationPackage::InovationPackage()
    : Package("inovation")
{
    patterns["."] = new ExpPattern(".|.|.|hand");
    patterns[".S"] = new ExpPattern(".|spade|.|hand");
    patterns[".C"] = new ExpPattern(".|club|.|hand");
    patterns[".H"] = new ExpPattern(".|heart|.|hand");
    patterns[".D"] = new ExpPattern(".|diamond|.|hand");

    patterns[".black"] = new ExpPattern(".|black|.|hand");
    patterns[".red"] = new ExpPattern(".|red|.|hand");

    patterns[".."] = new ExpPattern(".");
    patterns["..S"] = new ExpPattern(".|spade");
    patterns["..C"] = new ExpPattern(".|club");
    patterns["..H"] = new ExpPattern(".|heart");
    patterns["..D"] = new ExpPattern(".|diamond");

    patterns[".Basic"] = new ExpPattern("BasicCard");
    patterns[".Trick"] = new ExpPattern("TrickCard");
    patterns[".Equip"] = new ExpPattern("EquipCard");

    patterns[".Weapon"] = new ExpPattern("Weapon");
    patterns["slash"] = new ExpPattern("Slash");
    patterns["jink"] = new ExpPattern("Jink");
    patterns["peach"] = new  ExpPattern("Peach");
    patterns["nullification"] = new ExpPattern("Nullification");
    patterns["peach+analeptic"] = new ExpPattern("Peach,Analeptic");


    skills << new Keji << new Yingzi << new Paoxiao << new Tiaoxin << new Fankui << new Longdan << new Guicai << new Wumou << new Benghuai << new Fengbi << new GeneralSkillInvalidity << new Wansha;
    General *nagisa = new General(this, "Nagisa", "real", 3, false);
    nagisa->addSkill(new Guangyu);
    nagisa->addSkill(new GuangyuTrigger);
    nagisa->addSkill(new Xiyuan);
    nagisa->addSkill(new Chengmeng);
    related_skills.insertMulti("guangyu", "#guangyu-trigger");

    General *ushio = new General(this, "Ushio", "real", 3, false, true);
    ushio->addSkill(new Dingxin);

    General *tomoya = new General(this, "Tomoya", "real", 4, true);
    tomoya->addSkill(new Zhuren);
    tomoya->addSkill(new ZhurenTrigger);
    related_skills.insertMulti("zhuren", "#zhuren");
    tomoya->addSkill(new Daolu);
    skills << new Diangong << new DiangongTrigger << new Shouyang << new Haixing << new Tanyan << new ShouyangClear;
    related_skills.insertMulti("diangong", "#diangong");
    related_skills.insertMulti("shouyang", "#shouyang-clear");
    tomoya->addWakeTypeSkillForAudio("diangong");
    tomoya->addWakeTypeSkillForAudio("shouyang");
    tomoya->addWakeTypeSkillForAudio("haixing");
    tomoya->addWakeTypeSkillForAudio("tanyan");

    General *kyou = new General(this, "fKyou", "real", 4, false);
    kyou->addSkill(new Touzhi);
    kyou->addSkill(new Youjiao);

    General *Natsume_Rin = new General(this, "Natsume_Rin", "real", 99, false, false, false, 3);
    Natsume_Rin->addSkill(new Pasheng);
    Natsume_Rin->addSkill(new Maoqun);
    Natsume_Rin->addSkill(new Chengzhang);
    skills << new Zhiling << new ZhilingTrigger << new ZhilingMaxCards << new Zhixing;
    related_skills.insertMulti("zhiling", "#zhiling");
    related_skills.insertMulti("zhiling", "#zhiling-max");
    Natsume_Rin->addWakeTypeSkillForAudio("zhiling");
    Natsume_Rin->addWakeTypeSkillForAudio("SE_Zhixing");

    // for heg
    Natsume_Rin->addHegSkill(new MaoqunHeg);
    Natsume_Rin->addHegSkill("-SE_Pasheng");
    Natsume_Rin->addHegSkill("-SE_Maoqun");
    Natsume_Rin->addHegSkill("-SE_Chengzhang");
    Natsume_Rin->addHegSkill("zhiling");
    Natsume_Rin->addHegSkill("SE_Zhixing");
    Natsume_Rin->addHegWakeTypeSkillForAudio("-zhiling");
    Natsume_Rin->addHegWakeTypeSkillForAudio("-SE_Zhixing");

    General *KKotori = new General(this, "KKotori", "magic", 3, false);
    KKotori->addSkill(new Jianshi);
    KKotori->addSkill(new JianshiClear);
    related_skills.insertMulti("jianshi", "#jianshi-clear");
    KKotori->addSkill(new Qiyue);

    //General *Shizuru = new General(this, "Shizuru", "science", 3, false);
    //General *Saya = new General(this, "Saya", "real", 4, false);

    General *nao = new General(this, "Nao", "science", 3, false);
    nao->addSkill(new Huanxing);
    nao->addSkill(new Fushang);

    General *WSaki = new General(this, "WSaki", "science", 3, false);
    WSaki->addSkill(new Kuisi);
    WSaki->addSkill(new Youer);

    General *Nanami = new General(this, "Nanami", "real", 3, false);
    Nanami->addSkill(new Shengyou);
    Nanami->addSkill(new Jinqu);

    General *Mikoto = new General(this, "Mikoto", "science", 3, false);
    Mikoto->addSkill(new Paoji);
    Mikoto->addSkill(new Dianci);
    Mikoto->addSkill(new DianciProhibit);
    related_skills.insertMulti("dianci", "#dianci");

    General *Shana = new General(this, "Shana", "magic", 3, false);
    Shana->addSkill(new Zhena);
    Shana->addSkill(new Tianhuo);

    General *akarin = new General(this, "Akarin", "real", 3, false);
    akarin->addSkill(new SE_Touming);
    akarin->addSkill(new SE_ToumingClear);
    related_skills.insertMulti("SE_Touming", "#SE_Touming-clear");
    akarin->addSkill(new SE_Tuanzi);

    General *akari = new General(this, "Akari", "science", 3, false);
    akari->addSkill(new Takamakuri);
    akari->addSkill(new Tobiugachi);
    akari->addSkill(new Fukurouza);

    General *Koromo = new General(this, "Koromo", "real", 3, false);
    Koromo->addSkill(new Kongdi);
    Koromo->addSkill(new Yixiangting);

    General *Kaga = new General(this, "Kaga", "kancolle", 4, false);
    Kaga->addSkill(new Weishi);
   
    Kaga->addSkill(new Hongzha);
    Kaga->addSkill(new HongzhaClear);
    related_skills.insertMulti("hongzha", "#hongzha-clear");

    General *Kongou = new General(this, "Kongou", "kancolle", 4, false);
    Kongou->addSkill(new Nuequ);
    Kongou->addSkill(new BurningLove);


    General *Zuikaku = new General(this, "Zuikaku", "kancolle", 3, false);
    Zuikaku->addSkill(new Eryu);
    Zuikaku->addSkill(new EryuClear);
    related_skills.insertMulti("eryu", "#eryu-clear");
    Zuikaku->addSkill(new Zheyi);
    skills << new Youdiz;
    Zuikaku->addWakeTypeSkillForAudio("youdiz");

    //General *Shigure = new General(this, "Shigure", "kancolle", 3, false);
    General *Asashio = new General(this, "Asashio", "kancolle", 3, false);
    Asashio->addSkill(new Fanqian);
    Asashio->addSkill(new Buyu);
    //General *Nagato = new General(this, "Nagato", "kancolle", 4, false);
    General *Mogami = new General(this, "Mogami", "kancolle", 4, false);
    Mogami->addSkill(new Fanghuo);
    Mogami->addSkill(new Jianhun);
    Mogami->addSkill(new JianhunTargetMod);
    related_skills.insertMulti("jianhun", "#jianhun-target");
    //General *SaratogaR = new General(this, "SaratogaR", "kancolle", 4, false);
    //General *FubukiR = new General(this, "FubukiR", "kancolle", 3, false);
    General *AyanamiR = new General(this, "AyanamiR", "kancolle", 3, false);
    AyanamiR->addSkill(new Taxian);
    AyanamiR->addSkill(new Guishen);
    //General *QuincyR = new General(this, "QuincyR", "kancolle", 3, false);
    //General *AobaR = new General(this, "AobaR", "kancolle", 3, false);
    //General *Freyja = new General(this, "Freyja", "diva", 3, false);
    //General *Mikumo = new General(this, "Mikumo", "diva", 3, false);
    General *Ranka = new General(this, "Ranka", "diva", 3, false);
    Ranka->addSkill(new Xingjian);
    Ranka->addSkill(new XingjianClear);
    related_skills.insertMulti("xingjian", "#xingjian-clear");
    Ranka->addSkill(new Goutong);
    //General *Umi = new General(this, "Umi", "diva", 3, false);
    //General *Maki = new General(this, "Maki", "diva", 3, false);
    //General *Minori = new General(this, "Minori", "diva", 3, false);
    //General *Minoru = new General(this, "Minoru", "real", 4, true, true);
    //General *Hiroko = new General(this, "Hiroko", "diva", 3, false);
    //General *Youmu = new General(this, "Youmu", "touhou", 4, false);
    General *Sanae = new General(this, "Sanae", "touhou", 3, false);
    Sanae->addSkill(new Xinyang);
    Sanae->addSkill(new XinyangClear);
    related_skills.insertMulti("xinyang", "#xinyang-clear");
    Sanae->addSkill(new Fengzhu);
    //General *Yukari = new General(this, "Yukari", "touhou", 4, false);
    //General *Emilia = new General(this, "Emilia", "magic", 3, false);
    //General *Remu = new General(this, "Remu", "magic", 3, false);
    General *Mumei = new General(this, "Mumei", "science", 2, false);
    Mumei->addSkill(new Qinshi);
    Mumei->addSkill(new Kangfen);
    Mumei->addSkill(new Xiedou);
    General *Mine = new General(this, "Mine", "science", 3, false);
    Mine->addSkill(new Nangua);
    Mine->addSkill(new Jixian);
    //General *Akeno = new General(this, "Akeno", "science", 3, false);
    //General *Ako = new General(this, "Ako", "real", 3, false);
    General *NMakoto = new General(this, "NMakoto", "real", 4);
    NMakoto->addSkill(new Yandan);
    NMakoto->addSkill(new YandanClear);
    NMakoto->addSkill(new YandanMaxCards);
    related_skills.insertMulti("yandan", "#yandan");
    related_skills.insertMulti("yandan", "#yandan-clear");
    NMakoto->addSkill(new Xiwang);
    skills << new Lunpo << new LunpoInvalidity;
    related_skills.insertMulti("lunpo", "#lunpo-inv");
    NMakoto->addWakeTypeSkillForAudio("lunpo");

    General *Chiaki = new General(this, "Chiaki", "real", 3, false);
    Chiaki->addSkill(new Ningju);
    Chiaki->addSkill(new Zhinian);
    skills << new Chengxu;
    Chiaki->addWakeTypeSkillForAudio("chengxu");
    General *Shizuo = new General(this, "Shizuo", "real", 7);
    Shizuo->addSkill(new Baonu);
    Shizuo->addSkill(new Jizhanshiz);
    General *Nagi = new General(this, "Nagi", "real", 3, false);
    Nagi->addSkill(new Tianzi);
    Nagi->addSkill(new Yuzhai);
    General *Iroha = new General(this, "Iroha", "real", 3, false);
    Iroha->addSkill(new Jianjin);
    Iroha->addSkill(new Faka);
    General *Fear = new General(this, "Fear", "real", 3, false);
    Fear->addSkill(new Zuzhou);
    Fear->addSkill(new ZuzhouMaxCards);
    Fear->addSkill(new ZuzhouClear);
    Fear->addSkill(new Jiguan);
    Fear->addSkill(new JiguanClear);
    related_skills.insertMulti("zuzhou", "#zuzhou");
    related_skills.insertMulti("zuzhou", "#zuzhou-clear");
    related_skills.insertMulti("jiguan", "#jiguan-clear");

    General *Dalian = new General(this, "Dalian", "magic", 3, false);
    Dalian->addSkill(new Shuji);
    Dalian->addSkill(new ShujiMaxCards);
    related_skills.insertMulti("shuji", "#shuji");
    Dalian->addSkill(new Jicheng);

    General *Hugh = new General(this, "Hugh", "magic", 3, true, true);
    Hugh->addSkill(new Shoushi);
    Hugh->addSkill(new Kaiqi);



    /*
    General *kaori = new General(this, "Kaori", "real", 3, false);
    kaori->addSkill(new Chuangzao);
    kaori->addSkill(new Qidao);
    kaori->addSkill(new Benpao);
    skills << new Guangmang << new Shuohuang;
    kaori->addWakeTypeSkillForAudio("guangmang");
    kaori->addWakeTypeSkillForAudio("shuohuang");
    */
    General *sakura = new General(this, "DarkSakura1", "magic", 8, false, true);
    sakura->addSkill(new Xushu);
    sakura->addSkill(new Xishou);

    General *sakura2 = new General(this, "DarkSakura2", "magic", 4, false, true);
    sakura2->addSkill("xushu");
    sakura2->addSkill("xishou");
    sakura2->addSkill(new Shengbei);
    sakura2->addSkill(new ShengbeiMaxCards);
    related_skills.insertMulti("shengbei", "#shengbei");
    sakura2->addSkill(new Caoying);
    sakura2->addSkill(new ShengjianBlack);

    General *WalkerA = new General(this, "WalkerA", "real", 7, true, true);

    new General(this, "sujiang", "real", 4, true, true);
    new General(this, "sujiangf", "real", 4, false, true);

    new General(this, "anjiang", "real", 4, true, true, true);

    QList<Card *> cards;
    cards << new KeyTrick(Card::Heart, 10)
        << new KeyTrick(Card::Heart, 4)
        << new KeyTrick(Card::Diamond, 8)
        << new KeyTrick(Card::Spade, 11)
        << new KeyTrick(Card::Club, 1)
        << new MapoTofu(Card::Spade, 1);

    foreach(Card *card, cards)
        card->setParent(this);

    addMetaObject<TiaoxinCard>();
    addMetaObject<YinshenCard>();
    addMetaObject<QidaoCard>();
    addMetaObject<ShuohuangCard>();
    addMetaObject<ZhurenCard>();
    addMetaObject<DiangongCard>();
    addMetaObject<ZhilingCard>();
    addMetaObject<HongzhaCard>();
    addMetaObject<YouerCard>();
    addMetaObject<NuequCard>();
    addMetaObject<EryuCard>();
    addMetaObject<JizhanCard>();
    addMetaObject<TaxianCard>();
    addMetaObject<NingjuCard>();
    addMetaObject<FanqianCard>();
    addMetaObject<JiguanCard>();
    addMetaObject<PaojiCard>();
    addMetaObject<FengzhuCard>();
}

ADD_PACKAGE(Inovation)
