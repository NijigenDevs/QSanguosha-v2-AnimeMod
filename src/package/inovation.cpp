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
    damage.chain = false;
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
        events << EventPhaseStart << EventPhaseEnd;
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

        return false;
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
            room->broadcastSkillInvoke(objectName(), rand() % 2 + 1);
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
                room->broadcastSkillInvoke(objectName(), 3);
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
                room->broadcastSkillInvoke(objectName(), 4);
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
        events << Damaged << EventPhaseStart;
        frequency = NotFrequent;
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
            room->broadcastSkillInvoke(objectName(), rand() % 3 + 1);//1 2 3
            damage.to->gainMark("@fushang");
            damage.to->gainMark("@fushang_time", 2 - damage.to->getMark("@fushang_time"));
            return false;
        }
        else if (triggerEvent == EventPhaseStart) {
            if (player->getPhase() == Player::RoundStart){
                if (player->getMark("@fushang_time") > 0){
                    player->loseMark("@fushang_time");
                    if (player->getMark("@fushang_time") == 0){
                        room->broadcastSkillInvoke(objectName(), rand() % 2 + 4);//4 5
                        room->recover(player, RecoverStruct(player, NULL, player->getMark("@fushang")));
                        player->loseAllMarks("@fushang");
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

KeyTrick::KeyTrick(Card::Suit suit, int number)
    : DelayedTrick(suit, number)
{
    setObjectName("key_trick");
    mute = true;
    handling_method = Card::MethodNone;
}

bool KeyTrick::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const
{
    if (!targets.isEmpty() || to_select->containsTrick(objectName()))
        return false;
    return true;
}

void KeyTrick::takeEffect(ServerPlayer *) const
{
}

void KeyTrick::onEffect(const CardEffectStruct &) const
{
}

void KeyTrick::onNullified(ServerPlayer *) const
{
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
            do {
                if (!room->askForUseCard(player, "@@guangyu", "@guangyu-use")) break;
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
            room->changeHero(tomoya, "Ushio", false, true, true, true);
            LogMessage log;
            log.type = "#XiyuanChangeHero";
            log.from = death.who;
            log.to << tomoya;
            log.arg = objectName();
            room->sendLog(log);
            room->setTag("xiyuan_used", QVariant(true));
        }
    }

    bool triggerable(const ServerPlayer *target) const
    {
        return target != NULL && target->hasSkill(this);
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
            player->setFlags("dingxin_used");
            room->loseHp(player);
            player->setFlags("-dingxin_used");
        }
        else if (triggerEvent == Dying) {
            DyingStruct dying = data.value<DyingStruct>();
            if (!dying.who->hasSkill(objectName()) || !dying.who->hasFlag("dingxin_used"))
                return false;
            ServerPlayer *nagisa;
            foreach(ServerPlayer *p, room->getAllPlayers()){
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
                room->recover(dying.who, RecoverStruct(dying.who, NULL, 3));
                LogMessage log;
                log.type = "#DingxinRecover";
                log.from = dying.who;
                room->sendLog(log);
            }
            else{
                room->revivePlayer(nagisa, true);
                nagisa->setHp(2);
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


    skills << new Keji;
    General *nagisa = new General(this, "Nagisa", "real", 3, false);
    nagisa->addSkill(new Guangyu);
    nagisa->addSkill(new GuangyuTrigger);
    nagisa->addSkill(new Xiyuan);
    related_skills.insertMulti("guangyu", "#guangyu-trigger");

    General *ushio = new General(this, "Ushio", "real", 3, false, true);
    ushio->addSkill(new Dingxin);

    General *akarin = new General(this, "Akarin", "real", 3, false); 
    akarin->addSkill(new SE_Touming);
    akarin->addSkill(new SE_Tuanzi);
    General *nao = new General(this, "Nao", "science", 3, false);
    nao->addSkill(new Huanxing);
    nao->addSkill(new Fushang);

    QList<Card *> cards;
    cards << new KeyTrick(Card::Heart, 10)
        << new MapoTofu(Card::Spade, 1);

    foreach(Card *card, cards)
        card->setParent(this);

    addMetaObject<YinshenCard>();
}

ADD_PACKAGE(Inovation)

