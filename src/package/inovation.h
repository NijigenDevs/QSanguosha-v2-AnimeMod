#ifndef _INOVATION_H
#define _INOVATION_H

#include "package.h"
#include "card.h"
#include "standard.h"
#include "wind.h"

class InovationPackage : public Package
{
    Q_OBJECT

public:
    InovationPackage();
};

class TiaoxinCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE TiaoxinCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void onEffect(const CardEffectStruct &effect) const;
};


class MapoTofu : public BasicCard
{
    Q_OBJECT

public:
    Q_INVOKABLE MapoTofu(Card::Suit suit, int number);
    QString getSubtype() const;

    static bool IsAvailable(const Player *player, const Card *analeptic = NULL);

    bool isAvailable(const Player *player) const;
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void onUse(Room *room, const CardUseStruct &card_use) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
    void onEffect(const CardEffectStruct &effect) const;
};

class KeyTrick : public DelayedTrick
{
    Q_OBJECT

public:
    Q_INVOKABLE KeyTrick(Card::Suit suit, int number);

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void takeEffect(ServerPlayer *target) const;
    void onEffect(const CardEffectStruct &effect) const;
    void onNullified(ServerPlayer *target) const;
    void onUse(Room *room, const CardUseStruct &card_use) const;
};

class YinshenCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE YinshenCard();



    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class QidaoCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE QidaoCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class ShuohuangCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ShuohuangCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class ZhurenCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZhurenCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class DiangongCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE DiangongCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class ZhilingCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE ZhilingCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class HongzhaCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE HongzhaCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
    bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
};

class YouerCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE YouerCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class NuequCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE NuequCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class EryuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE EryuCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class JizhanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JizhanCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class TaxianCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE TaxianCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
    bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
};

class NingjuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE NingjuCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class FanqianCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE FanqianCard();

    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class JiguanCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE JiguanCard();

    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};


class PaojiCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE PaojiCard();

    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    void use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const;
};

class FengzhuDialog : public GuhuoDialog
{
    Q_OBJECT

public:
    static FengzhuDialog *getInstance();

protected:
    explicit FengzhuDialog();
    virtual bool isButtonEnabled(const QString &button_name) const;
};

class FengzhuCard : public SkillCard
{
    Q_OBJECT

public:
    Q_INVOKABLE FengzhuCard();
    bool targetFixed() const;
    bool targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const;
    bool targetsFeasible(const QList<const Player *> &targets, const Player *Self) const;
    const Card *validate(CardUseStruct &cardUse) const;
    const Card *validateInResponse(ServerPlayer *user) const;
};


#endif

