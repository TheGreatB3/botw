#include "Game/Cooking/cookManager.h"
#include <codec/seadHashCRC32.h>
#include <random/seadGlobalRandom.h>
#include <typeindex>
#include "KingSystem/ActorSystem/actInfoData.h"
#include "KingSystem/Resource/resLoadRequest.h"
#include "KingSystem/Utils/InitTimeInfo.h"

namespace uking {

struct CookingEffect {
    sead::SafeString name;
    CookEffectId effect_id;
};

static const CookingEffect sCookingEffects[CookingMgr::NumEffects]{
    {"None", CookEffectId::None},
    {"LifeRecover", CookEffectId::LifeRecover},
    {"LifeMaxUp", CookEffectId::LifeMaxUp},
    {"ResistHot", CookEffectId::ResistHot},
    {"ResistCold", CookEffectId::ResistCold},
    {"ResistElectric", CookEffectId::ResistElectric},
    {"AttackUp", CookEffectId::AttackUp},
    {"DefenseUp", CookEffectId::DefenseUp},
    {"Quietness", CookEffectId::Quietness},
    {"MovingSpeed", CookEffectId::MovingSpeed},
    {"GutsRecover", CookEffectId::GutsRecover},
    {"ExGutsMaxUp", CookEffectId::ExGutsMaxUp},
    {"Fireproof", CookEffectId::Fireproof},
};

struct Crc32Constants {
    ksys::util::InitConstants init_constants;
    const u32 crc32_life_recover = sead::HashCRC32::calcStringHash("LifeRecover");
    const u32 crc32_guts_performance = sead::HashCRC32::calcStringHash("GutsPerformance");
    const u32 crc32_stamina_recover = sead::HashCRC32::calcStringHash("StaminaRecover");
    const u32 crc32_life_max_up = sead::HashCRC32::calcStringHash("LifeMaxUp");
    const u32 crc32_resist_hot = sead::HashCRC32::calcStringHash("ResistHot");
    const u32 crc32_resist_cold = sead::HashCRC32::calcStringHash("ResistCold");
    const u32 crc32_resist_electric = sead::HashCRC32::calcStringHash("ResistElectric");
    const u32 crc32_all_speed = sead::HashCRC32::calcStringHash("AllSpeed");
    const u32 crc32_attack_up = sead::HashCRC32::calcStringHash("AttackUp");
    const u32 crc32_defense_up = sead::HashCRC32::calcStringHash("DefenseUp");
    const u32 crc32_quietness = sead::HashCRC32::calcStringHash("Quietness");
    const u32 crc32_fireproof = sead::HashCRC32::calcStringHash("Fireproof");
};

static Crc32Constants sCrc32Constants;

CookItem::CookItem() = default;

void CookItem::reset() {
    actor_name.clear();
    life_recover = 0.0f;
    effect_time = 0;
    _224 = false;
    item_price = 0;
    effect_id = CookEffectId::None;
    stamina_recover = 0.0f;
    for (auto& ingredient : ingredients) {
        ingredient.clear();
    }
}

void CookItem::copy(CookItem& to) const {
    to.actor_name = actor_name;
    to.life_recover = life_recover;
    to.effect_time = effect_time;
    to._224 = _224;
    to.item_price = item_price;
    to.stamina_recover = stamina_recover;
    to.effect_id = effect_id;
    to.ingredients = ingredients;
}

SEAD_SINGLETON_DISPOSER_IMPL(CookingMgr)

CookingMgr::CookingMgr() = default;

CookingMgr::~CookingMgr() {
    if (mConfig) {
        delete mConfig;
        mConfig = nullptr;
    }
}

void CookingMgr::cookFail(CookItem& item) {
    if (item.actor_name.isEmpty())
        item.actor_name.copy(mFailActorName);

    f32 life_recover;
    if (item.actor_name == mFailActorName) {
        // Dubious food
        item.effect_time = 0;
        const f32 min_recovery = (f32)(s32)mFALR;
        life_recover = min_recovery > item.life_recover ? min_recovery : item.life_recover;
    } else {
        // Rock-hard food
        item.effect_time = 0;
        life_recover = (f32)(s32)mSFALR;
    }

    item.life_recover = life_recover;
    item.stamina_recover = 0.0f;
    item.effect_id = CookEffectId::None;
    item.item_price = 2;
}

void CookingMgr::cookFailForMissingConfig(CookItem& item, const sead::SafeString& name) {
    f32 life_recover;
    if (name.isEmpty() || name == mFailActorName) {
        item.actor_name.copy(mFailActorName);
        item.effect_time = 0;
        life_recover = (f32)(s32)mFALR;
    } else {
        item.actor_name = name;
        item.effect_time = 0;
        life_recover = (f32)(s32)mSFALR;
    }

    item.life_recover = life_recover;
    item.stamina_recover = 0.0f;
    item.effect_id = CookEffectId::None;
    item.item_price = 1;
}

void CookingMgr::cookCalcBoost(const IngredientArray& ingredients, CookItem& item,
                               const BoostArg* boost_arg) const {
    // Find if any of the ingredients are Monster Extract.
    for (int i = 0; i < NumIngredientsMax; i++) {
        const auto& ingredient = ingredients[i];
        if (ingredient.arg && ingredient.arg->name == mMonsterExtractName) {
            cookHandleBoostMonsterExtractInner(ingredients, item);
            return;
        }
    }

    if (!boost_arg || !boost_arg->always_boost) {
        if (boost_arg && !boost_arg->enable_random_boost) {
            return;
        }

        s32 success_rate = 0;
        s32 num_ingredients = 0;
        s32 threshold = 0;

        for (int i = 0; i < NumIngredientsMax; i++) {
            const auto& ingredient = ingredients[i];
            if (ingredient.arg) {
                num_ingredients++;
                if (ingredient.actor_data.tryGetIntByKey(&success_rate,
                                                         "cookSpiceBoostSuccessRate") &&
                    success_rate > threshold) {
                    threshold = success_rate;
                }
            }
        }

        if (num_ingredients >= 1)
            threshold += mNMSSR[num_ingredients - 1];

        if ((s32)sead::GlobalRandom::instance()->getU32(100) >= threshold)
            return;
    }
    cookHandleBoostSuccessInner(ingredients, item);
}

void CookingMgr::cookHandleBoostMonsterExtractInner(
    [[maybe_unused]] const IngredientArray& ingredients, CookItem& item) const {
    // Monster Extract found; calculate boosts.

    s32 effect_min = 0;
    s32 effect_max = 4;

    if (item.life_recover <= 0.0f || item.effect_id == CookEffectId::LifeMaxUp)
        effect_min = 2;

    if (item.effect_id == CookEffectId::None)
        effect_max = 2;

    switch (sead::GlobalRandom::instance()->getS32Range(effect_min, effect_max)) {
    case 0:
        item.life_recover += (f32)mCookingEffectEntries[(int)CookEffectId::LifeRecover].ssa;
        break;
    case 1:
        item.life_recover = (f32)mCookingEffectEntries[(int)CookEffectId::LifeRecover].mi;
        break;
    case 2:
        if (item.effect_id != CookEffectId::None) {
            if (item.stamina_recover > 0.0f && item.stamina_recover < 1.0f) {
                item.stamina_recover = 1.0;
            }
            item.stamina_recover =
                (f32)((s32)item.stamina_recover + mCookingEffectEntries[(int)item.effect_id].ssa);
        }
        break;
    case 3:
        if (item.effect_id != CookEffectId::None) {
            item.stamina_recover = (f32)mCookingEffectEntries[(int)item.effect_id].mi;
        }
        break;
    default:
        break;
    }

    // Effect time
    if (item.effect_time >= 1) {
        const u32 roll = sead::GlobalRandom::instance()->getU32(3);

        if (roll == 0)
            // 1 minute
            item.effect_time = 60;
        if (roll == 1)
            // 10 minutes
            item.effect_time = 600;
        if (roll == 2)
            // 30 minutes
            item.effect_time = 1800;
    }
}

void CookingMgr::cookHandleBoostSuccessInner([[maybe_unused]] const IngredientArray& ingredients,
                                             CookItem& item) const {
    enum Bonus {
        LifeBonus = 0,
        StaminaBonus = 1,
        TimeBonus = 2,
    };

    Bonus bonus = LifeBonus;

    item._224 = true;

    if (item.effect_id != CookEffectId::None) {
        const f32 life_recover = item.life_recover;
        const s32 stamina_recover = sead::Mathi::clampMin((s32)item.stamina_recover, 1);

        const f32 life_recover_ma = (f32)mCookingEffectEntries[(int)CookEffectId::LifeRecover].ma;
        const s32 stamina_recover_ma = mCookingEffectEntries[(int)item.effect_id].ma;

        const bool life_recover_maxed = life_recover >= life_recover_ma;
        const bool stamina_recover_maxed = stamina_recover >= stamina_recover_ma;

        switch (item.effect_id) {
        case CookEffectId::GutsRecover:
        case CookEffectId::ExGutsMaxUp:
            if (stamina_recover_maxed)
                bonus = LifeBonus;
            else if (life_recover_maxed)
                bonus = StaminaBonus;
            else
                bonus = sead::GlobalRandom::instance()->getBool() ? StaminaBonus : LifeBonus;
            break;

        case CookEffectId::LifeMaxUp:
            bonus = StaminaBonus;
            break;

        default:
            if (!stamina_recover_maxed) {
                if (life_recover_maxed) {
                    bonus = sead::GlobalRandom::instance()->getBool() ? TimeBonus : StaminaBonus;
                } else {
                    bonus = (Bonus)sead::GlobalRandom::instance()->getU32(3);
                }
            } else if (life_recover_maxed) {
                bonus = TimeBonus;
            } else {
                bonus = sead::GlobalRandom::instance()->getBool() ? TimeBonus : LifeBonus;
            }
            break;
        }
    }

    switch (bonus) {
    case StaminaBonus:
        if (item.effect_id != CookEffectId::None) {
            if (item.stamina_recover > 0.0f && item.stamina_recover < 1.0f)
                item.stamina_recover = 1.0f;
            item.stamina_recover =
                (f32)((int)item.stamina_recover + mCookingEffectEntries[(int)item.effect_id].ssa);
        }
        break;
    case TimeBonus:
        item.effect_time += (s32)mSSAET;
        break;
    case LifeBonus:
        item.life_recover += (f32)mCookingEffectEntries[(int)CookEffectId::LifeRecover].ssa;
        break;
    }
}

void CookingMgr::cookCalcSpiceBoost(const IngredientArray& ingredients, CookItem& item) const {
    for (int i = 0; i < NumIngredientsMax; i++) {
        if (ingredients[i].arg) {
            const auto& actor_data = ingredients[i].actor_data;
            if (!ksys::act::InfoData::instance()->hasTag(actor_data, ksys::act::tags::CookEnemy) &&
                ksys::act::InfoData::instance()->hasTag(actor_data, ksys::act::tags::CookSpice)) {
                int int_val = 0;

                if (actor_data.tryGetIntByKey(&int_val, "cookSpiceBoostHitPointRecover") &&
                    int_val > 0) {
                    item.life_recover += (f32)int_val;
                }

                if (actor_data.tryGetIntByKey(&int_val, "cookSpiceBoostEffectiveTime") &&
                    int_val > 0) {
                    item.effect_time += int_val;
                }

                if (actor_data.tryGetIntByKey(&int_val, "cookSpiceBoostMaxHeartLevel") &&
                    int_val > 0 && i <= 0) {
                    if (item.effect_id == CookEffectId::LifeMaxUp) {
                        while (i < 1) {
                            item.stamina_recover += (f32)int_val;
                            i++;
                        }
                    } else {
                        i = 1;
                    }
                }

                if (ingredients[i].actor_data.tryGetIntByKey(&int_val,
                                                             "cookSpiceBoostStaminaLevel") &&
                    int_val > 0 && i <= 0) {
                    if (item.effect_id == CookEffectId::GutsRecover ||
                        item.effect_id == CookEffectId::ExGutsMaxUp) {
                        while (i < 1) {
                            item.stamina_recover += (f32)int_val;
                            i++;
                        }
                    } else {
                        i = 1;
                    }
                }
            }
        }
    }
}

void CookingMgr::cookCalcItemPrice(const IngredientArray& ingredients, CookItem& item) const {
    item.item_price = 0;
    s32 price;

    if (mFairyTonicName == item.actor_name) {
        item.item_price = 2;
        return;
    }

    s32 int_val = 0;
    s32 max_price = 0;
    s32 mult_idx = 0;

    for (int i = 0; i < NumIngredientsMax; ++i) {
        const auto& ingredient = ingredients[i];
        const auto& actor_data = ingredient.actor_data;

        if (!ingredient.arg)
            break;

        if (ksys::act::InfoData::instance()->hasTag(actor_data, ksys::act::tags::CookLowPrice)) {
            // This ingredient is only worth 1 rupee.
            const s32 count = ingredient.arg->count;
            mult_idx += count;
            item.item_price += count;
            max_price += ingredient.arg->count;
        } else {
            if (actor_data.tryGetIntByKey(&int_val, "itemSellingPrice")) {
                const s32 count = ingredient.arg->count;
                mult_idx += count;
                item.item_price += int_val * count;
            }
            if (actor_data.tryGetIntByKey(&int_val, "itemBuyingPrice")) {
                max_price += int_val * ingredient.arg->count;
            }
        }
    }

    if (mult_idx >= 1) {
        price = (s32)(mNMMR[mult_idx - 1] * (float)item.item_price);
        item.item_price = price;
    } else {
        price = item.item_price;
    }

    if (price >= 1) {
        // Round up to the nearest power of 10
        if (price % 10 != 0) {
            price = price + 10 - price % 10;
            item.item_price = price;
        }
    }

    if (max_price < price)
        price = max_price;
    if (price <= 2)
        price = 2;

    item.item_price = price;
}

void CookingMgr::cookCalcPotencyBoost(const IngredientArray& ingredients, CookItem& item) const {
    const bool is_not_medicine =
        item.actor_name.isEmpty() || !ksys::act::InfoData::instance()->hasTag(
                                         item.actor_name.cstr(), ksys::act::tags::CookEMedicine);
    const bool is_not_fairy_tonic = mFairyTonicName != item.actor_name;

    sead::SafeArray<s32, NumEffectSlots> effect_counts{};
    sead::SafeArray<s32, NumEffectSlots> cure_levels{};

    s32 stamina_boost = 0;
    s32 life_boost = 0;
    s32 time_boost = 0;
    s32 total_count = 0;
    s32 life_recover = 0;

    for (int i = 0; i < NumIngredientsMax; i++) {
        if (!ingredients[i].arg)
            break;
        const al::ByamlIter& actor_data = ingredients[i].actor_data;
        const s32 count = ingredients[i].arg->count;
        total_count += count;
        int int_val;
        if (ksys::act::InfoData::instance()->hasTag(actor_data, ksys::act::tags::CookEnemy)) {
            if (actor_data.tryGetIntByKey(&int_val, "cookSpiceBoostEffectiveTime") && int_val > 0) {
                time_boost += int_val * count;
            }

            if (actor_data.tryGetIntByKey(&int_val, "cookSpiceBoostMaxHeartLevel") && int_val > 0) {
                life_boost += int_val * count;
            }

            if (actor_data.tryGetIntByKey(&int_val, "cookSpiceBoostStaminaLevel") && int_val > 0) {
                stamina_boost += int_val * count;
            }
        } else {
            if (actor_data.tryGetIntByKey(&int_val, "cureItemHitPointRecover") && int_val > 0) {
                life_recover += int_val * count;
            }

            if (actor_data.tryGetIntByKey(&int_val, "cureItemEffectLevel") && int_val > 0) {
                const char* string_val = nullptr;
                if (actor_data.tryGetStringByKey(&string_val, "cureItemEffectType")) {
                    const auto effect_id = getCookEffectId(string_val);
                    if (effect_id != CookEffectId::None) {
                        effect_counts[(int)effect_id] += count;
                        cure_levels[(int)effect_id] += int_val * count;
                    }
                }
            }
        }
    }

    bool effect_found = false;
    for (int i = 0; i < NumEffectSlots; i++) {
        const s32 count = effect_counts[i];
        if (count > 0) {
            if (effect_found) {
                // Finding a second effect makes them cancel out.
                effect_found = false;
                item.stamina_recover = 0.0f;
                item.effect_id = CookEffectId::None;
                item.effect_time = 0;
                break;
            }

            const auto& entry = mCookingEffectEntries[i];

            const f32 cure_level = (f32)cure_levels[i];
            item.stamina_recover = cure_level * entry.mr;

            const auto effect_id = (CookEffectId)i;
            item.effect_id = effect_id;

            const s32 boost_time = entry.bt;
            if (boost_time > 0)
                item.effect_time = time_boost + 30 * total_count + boost_time * count;

            if (effect_id == CookEffectId::LifeMaxUp) {
                item.stamina_recover += (f32)life_boost;
            } else if (effect_id == CookEffectId::GutsRecover ||
                       effect_id == CookEffectId::ExGutsMaxUp) {
                item.stamina_recover += (f32)stamina_boost;
            }
            effect_found = true;
        }
    }

    if (!is_not_fairy_tonic && effect_found) {
        effect_found = false;
        item.stamina_recover = 0.0f;
        item.effect_id = CookEffectId::None;
        item.effect_time = 0;
    }

    if (!is_not_medicine && !effect_found) {
        item.actor_name = mFailActorName;
    }

    const bool is_failure = !item.actor_name.isEmpty() &&
                            ksys::act::InfoData::instance()->hasTag(item.actor_name.cstr(),
                                                                    ksys::act::tags::CookFailure);
    if (is_failure) {
        item.life_recover = (f32)life_recover * mFALRMR;
    } else if (effect_found) {
        item.life_recover = (f32)life_recover * mLRMR;
    } else {
        item.life_recover =
            (f32)life_recover * mCookingEffectEntries[(int)CookEffectId::LifeRecover].mr;
    }

    if (item.effect_id != CookEffectId::None) {
        const s32 max = mCookingEffectEntries[(int)item.effect_id].ma;
        if (item.stamina_recover > (f32)max)
            item.stamina_recover = (f32)max;
    }
}

CookEffectId CookingMgr::getCookEffectId(const sead::SafeString& name) const {
    const auto name_hash = sead::HashCRC32::calcStringHash(name);
    return [&] {
        if (const auto* node = mCookingEffectNameIdMap.find(name_hash)) {
            return node->value();
        }
        return CookEffectId::None;
    }();
}

void CookingMgr::init(sead::Heap* heap) {
    ksys::res::LoadRequest req;

    req.mRequester = "CookingMgr";
    req._22 = false;

    sead::FixedSafeString<0x80> path;
    path.format("Cooking/CookData.byml");

    auto* res = sead::DynamicCast<sead::DirectResource>(mResHandle.load(path, &req));
    if (!res)
        return;

    mConfig = mConfig ? new (mConfig) al::ByamlIter(res->getRawData()) :
                        new (heap) al::ByamlIter(res->getRawData());

    mCookingEffectNameIdMap.clear();

    for (int effect_idx = 0; effect_idx < NumEffects; effect_idx++) {
        auto& effect = sCookingEffects[effect_idx];
        const u32 name_hash = sead::HashCRC32::calcStringHash(effect.name);
        mCookingEffectNameIdMap.insert(name_hash, effect.effect_id);
    }

    for (int i = 0; i < NumEffectSlots; i++) {
        mCookingEffectEntries[i] = CookingEffectEntry{};
        mCookingEffectEntries[i].ssa = 0;
    }

    mNMMR[0] = 1.0f;
    mNMMR[1] = 1.0f;
    mNMMR[2] = 1.0f;
    mNMMR[3] = 1.0f;
    mNMMR[4] = 1.0f;

    mNMSSR[0] = 0;
    mNMSSR[1] = 5;
    mNMSSR[2] = 10;
    mNMSSR[3] = 15;
    mNMSSR[4] = 20;

    mFairyTonicName = "Item_Cook_C_16";
    mFairyTonicNameHash = sead::HashCRC32::calcStringHash(mFairyTonicName);

    mFailActorName = "Item_Cook_O_01";
    mFailActorNameHash = sead::HashCRC32::calcStringHash(mFailActorName);

    mMonsterExtractName = "Item_Material_08";
    mMonsterExtractNameHash = sead::HashCRC32::calcStringHash(mMonsterExtractName);

    mLRMR = 1.0;
    mSFALR = 1;
    mSSAET = 300;
    mFALRMR = 1.0;
    mFALR = 4;

    al::ByamlIter iter;
    al::ByamlIter cei_iter;
    al::ByamlIter entry_iter;

    const char* string_val = nullptr;
    int int_val;
    u32 uint_val;
    float float_val;

    if (mConfig->tryGetIterByKey(&iter, "System")) {
        if (iter.tryGetStringByKey(&string_val, "FA")) {
            mFailActorName = string_val;
            mFailActorNameHash = sead::HashCRC32::calcStringHash(mFailActorName);
        }
        if (iter.tryGetStringByKey(&string_val, "FCA")) {
            mFairyTonicName = string_val;
            mFairyTonicNameHash = sead::HashCRC32::calcStringHash(mFairyTonicName);
        }
        if (iter.tryGetStringByKey(&string_val, "MEA")) {
            mMonsterExtractName = string_val;
            mMonsterExtractNameHash = sead::HashCRC32::calcStringHash(mMonsterExtractName);
        }

        if (iter.tryGetFloatByKey(&float_val, "LRMR") && float_val >= 0)
            mLRMR = float_val;
        if (iter.tryGetFloatByKey(&float_val, "FALRMR") && float_val >= 0)
            mFALRMR = float_val;
        if (iter.tryGetIntByKey(&int_val, "FALR") && int_val >= 0)
            mFALR = int_val;
        if (iter.tryGetIntByKey(&int_val, "SFALR") && int_val >= 0)
            mSFALR = int_val;
        if (iter.tryGetIntByKey(&int_val, "SSAET") && int_val >= 0)
            mSSAET = int_val;

        if (iter.tryGetIterByKey(&cei_iter, "CEI")) {
            const int size = cei_iter.getSize();

            for (int i = 0; i < size; i++) {
                if (cei_iter.tryGetIterByIndex(&entry_iter, i) &&
                    entry_iter.tryGetUIntByKey(&uint_val, "T")) {
                    const u32 entry_hash = uint_val;

                    CookEffectId entry_idx;

                    if (sCrc32Constants.crc32_life_recover == entry_hash)
                        entry_idx = CookEffectId::LifeRecover;
                    else if (sCrc32Constants.crc32_guts_performance == entry_hash)
                        entry_idx = CookEffectId::ExGutsMaxUp;
                    else if (sCrc32Constants.crc32_stamina_recover == entry_hash)
                        entry_idx = CookEffectId::GutsRecover;
                    else if (sCrc32Constants.crc32_life_max_up == entry_hash)
                        entry_idx = CookEffectId::LifeMaxUp;
                    else if (sCrc32Constants.crc32_resist_hot == entry_hash)
                        entry_idx = CookEffectId::ResistHot;
                    else if (sCrc32Constants.crc32_resist_cold == entry_hash)
                        entry_idx = CookEffectId::ResistCold;
                    else if (sCrc32Constants.crc32_resist_electric == entry_hash)
                        entry_idx = CookEffectId::ResistElectric;
                    else if (sCrc32Constants.crc32_all_speed == entry_hash)
                        entry_idx = CookEffectId::MovingSpeed;
                    else if (sCrc32Constants.crc32_attack_up == entry_hash)
                        entry_idx = CookEffectId::AttackUp;
                    else if (sCrc32Constants.crc32_defense_up == entry_hash)
                        entry_idx = CookEffectId::DefenseUp;
                    else if (sCrc32Constants.crc32_quietness == entry_hash)
                        entry_idx = CookEffectId::Quietness;
                    else if (sCrc32Constants.crc32_fireproof == entry_hash)
                        entry_idx = CookEffectId::Fireproof;
                    else
                        continue;

                    if (entry_iter.tryGetIntByKey(&int_val, "BT"))
                        mCookingEffectEntries[(s32)entry_idx].bt = int_val;

                    if (entry_iter.tryGetIntByKey(&int_val, "Ma"))
                        mCookingEffectEntries[(s32)entry_idx].ma = int_val;

                    if (entry_iter.tryGetIntByKey(&int_val, "Mi"))
                        mCookingEffectEntries[(s32)entry_idx].mi = int_val;

                    if (entry_iter.tryGetFloatByKey(&float_val, "MR"))
                        mCookingEffectEntries[(s32)entry_idx].mr = float_val;

                    if (entry_iter.tryGetIntByKey(&int_val, "SSA"))
                        mCookingEffectEntries[(s32)entry_idx].ssa = int_val;
                }
            }
        }

        if (iter.tryGetIterByKey(&cei_iter, "NMMR")) {
            const int size = cei_iter.getSize();

            for (int i = 0; i < size; i++) {
                if (cei_iter.tryGetFloatByIndex(&float_val, i) && i < NumIngredientsMax) {
                    mNMMR[i] = sead::Mathf::clamp(float_val, 0.0f, 5.0f);
                }
            }
        }

        if (iter.tryGetIterByKey(&cei_iter, "NMSSR")) {
            const int size = cei_iter.getSize();

            for (int i = 0; i < size; i++) {
                if (cei_iter.tryGetIntByIndex(&int_val, i) && i < NumIngredientsMax) {
                    mNMSSR[i] = sead::Mathi::clamp(int_val, -100, 100);
                }
            }
        }
    }
}

bool CookingMgr::cook(const CookArg& arg, CookItem& cook_item,
                      const CookingMgr::BoostArg& boost_arg) {
    al::ByamlIter recipes_iter;
    sead::SafeArray<Ingredient, NumIngredientsMax> ingredients;

    if (!mConfig || !ksys::act::InfoData::instance()) {
    COOK_FAILURE_FOR_MISSING_CONFIG:
        cookFailForMissingConfig(cook_item, mFailActorName);
        return false;
    }

    if (false) {
    BAD_RECIPE:
        cook_item.actor_name = mFailActorName;
        cookCalcPotencyBoost(ingredients, cook_item);

    COOK_FAILURE:
        cookFail(cook_item);
        return true;
    }

    int num_ingredients = 0;
    for (int i = 0; i < NumIngredientsMax; i++) {
        if (!arg.ingredients[i].name.isEmpty()) {
            const u32 name_hash = sead::HashCRC32::calcStringHash(arg.ingredients[i].name);
            if (ksys::act::InfoData::instance()->getActorIter(&ingredients[i].actor_data,
                                                              name_hash)) {
                num_ingredients++;
                ingredients[i].name_hash = name_hash;
                ingredients[i].arg = &arg.ingredients[i];
            }
        }
    }

    if (num_ingredients == 0) {
        goto COOK_FAILURE_FOR_MISSING_CONFIG;
    }

    if (num_ingredients > 1) {
        if (mConfig->tryGetIterByKey(&recipes_iter, "Recipes")) {
            const s32 num_recipes = recipes_iter.getSize();
            const char* string_val = nullptr;
            u32 uint_val = 0;
            al::ByamlIter recipe_iter;
            al::ByamlIter actor_tag_iter;
            al::ByamlIter actors_iter;
            al::ByamlIter tags_iter;

            if (num_recipes > 0) {
                for (int recipe_idx = 0; recipe_idx < num_recipes; recipe_idx++) {
                    if (!recipes_iter.tryGetIterByIndex(&recipe_iter, recipe_idx))
                        continue;

                    recipe_iter.tryGetStringByKey(&string_val, "Result");
                    recipe_iter.tryGetUIntByKey(&uint_val, "Recipe");
                    const s32 num_actors = recipe_iter.tryGetIterByKey(&actors_iter, "Actors") ?
                                               actors_iter.getSize() :
                                               0;
                    const s32 num_tags =
                        recipe_iter.tryGetIterByKey(&tags_iter, "Tags") ? tags_iter.getSize() : 0;

                    if (num_actors + num_tags > num_ingredients ||
                        (num_actors == 0 && num_tags == 0))
                        continue;

                    ingredients[0]._10 = false;
                    ingredients[1]._10 = false;
                    ingredients[2]._10 = false;
                    ingredients[3]._10 = false;
                    ingredients[4]._10 = false;

                    for (int actor_idx = 0; actor_idx < num_actors; actor_idx++) {
                        bool found = false;
                        if (actors_iter.tryGetIterByIndex(&actor_tag_iter, actor_idx)) {
                            const s32 num_hashes = actor_tag_iter.getSize();
                            for (int hash_idx = 0; hash_idx < num_hashes; hash_idx++) {
                                u32 hash_val;
                                if (actor_tag_iter.tryGetUIntByIndex(&hash_val, hash_idx)) {
                                    for (int ingredient_idx = 0; ingredient_idx < num_ingredients;
                                         ingredient_idx++) {
                                        if (!ingredients[ingredient_idx]._10 &&
                                            ingredients[ingredient_idx].name_hash == hash_val) {
                                            ingredients[ingredient_idx]._10 = true;
                                            found = true;
                                            break;
                                        }
                                    }
                                }
                                if (found)
                                    break;
                            }
                        }
                        if (found)
                            break;
                    }

                    for (int tag_idx = 0; tag_idx < num_tags; tag_idx++) {
                        bool found = false;
                        if (tags_iter.tryGetIterByIndex(&actor_tag_iter, tag_idx)) {
                            const s32 num_hashes = actor_tag_iter.getSize();
                            for (int hash_idx = 0; hash_idx < num_hashes; hash_idx++) {
                                u32 hash_val;
                                if (actor_tag_iter.tryGetUIntByIndex(&hash_val, hash_idx)) {
                                    for (int ingredient_idx = 0; ingredient_idx < num_ingredients;
                                         ingredient_idx++) {
                                        if (!ingredients[ingredient_idx]._10 &&
                                            ingredients[ingredient_idx].name_hash == hash_val) {
                                            ingredients[ingredient_idx]._10 = true;
                                            found = true;
                                            break;
                                        }
                                    }
                                }
                                if (found)
                                    break;
                            }
                        }
                        if (found)
                            break;
                    }

                    al::ByamlIter actor_iter;
                    if (!ksys::act::InfoData::instance()->getActorIter(&actor_iter, uint_val))
                        continue;
                    actor_iter.tryGetStringByKey(&string_val, "name");
                    cook_item.actor_name = string_val;
                    cookCalcPotencyBoost(ingredients, cook_item);
                    if (ksys::act::InfoData::instance()->hasTag(cook_item.actor_name.cstr(),
                                                                ksys::act::tags::CookFailure)) {
                        goto COOK_FAILURE;
                    }

                    cookCalcBoost(ingredients, cook_item, &boost_arg);
                    cookCalcSpiceBoost(ingredients, cook_item);

                    int int_val;

                    if (recipe_iter.tryGetIntByKey(&int_val, "HB"))
                        cook_item.life_recover += (f32)int_val;

                    if (recipe_iter.tryGetIntByKey(&int_val, "TB")) {
                        if (cook_item.effect_time > 0) {
                            cook_item.effect_time += int_val;
                        }
                    }

                    cookAdjustItem(cook_item);

                    cookCalcItemPrice(ingredients, cook_item);
                    return true;
                }
            }
        }
    } else if (num_ingredients == 1) {
        if (mConfig->tryGetIterByKey(&recipes_iter, "SingleRecipes")) {
            const s32 num_recipes = recipes_iter.getSize();
            const char* string_val = nullptr;
            s32 int_val;
            u32 uint_val = 0;
            al::ByamlIter recipe_iter;
            al::ByamlIter actors_iter;
            al::ByamlIter tags_iter;

            if (num_recipes > 0) {
                for (int recipe_idx = 0; recipe_idx < num_recipes; recipe_idx++) {
                    if (!recipes_iter.tryGetIterByIndex(&recipe_iter, recipe_idx))
                        continue;

                    recipe_iter.tryGetStringByKey(&string_val, "Result");
                    recipe_iter.tryGetUIntByKey(&uint_val, "Recipe");

                    if (!recipe_iter.tryGetIntByKey(&int_val, "Num") || num_ingredients < int_val)
                        continue;

                    const s32 num_actors = recipe_iter.tryGetIterByKey(&actors_iter, "Actors") ?
                                               actors_iter.getSize() :
                                               0;
                    const s32 num_tags =
                        recipe_iter.tryGetIterByKey(&tags_iter, "Tags") ? tags_iter.getSize() : 0;

                    if (num_actors + num_tags > num_ingredients ||
                        (num_actors == 0 && num_tags == 0))
                        continue;

                    for (int actor_idx = 0; actor_idx < num_actors; actor_idx++) {
                        bool found = false;
                        const s32 num_hashes = actors_iter.getSize();
                        for (int hash_idx = 0; hash_idx < num_hashes; hash_idx++) {
                            u32 hash_val;
                            if (actors_iter.tryGetUIntByIndex(&hash_val, hash_idx)) {
                                for (int ingredient_idx = 0; ingredient_idx < num_ingredients;
                                     ingredient_idx++) {
                                    if (!ingredients[ingredient_idx]._10 &&
                                        ingredients[ingredient_idx].name_hash == hash_val) {
                                        ingredients[ingredient_idx]._10 = true;
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            if (found)
                                break;
                        }
                        if (found)
                            break;
                    }

                    for (int tag_idx = 0; tag_idx < num_tags; tag_idx++) {
                        bool found = false;
                        const s32 num_hashes = tags_iter.getSize();
                        for (int hash_idx = 0; hash_idx < num_hashes; hash_idx++) {
                            u32 hash_val;
                            if (tags_iter.tryGetUIntByIndex(&hash_val, hash_idx)) {
                                for (int ingredient_idx = 0; ingredient_idx < num_ingredients;
                                     ingredient_idx++) {
                                    if (!ingredients[ingredient_idx]._10 &&
                                        ingredients[ingredient_idx].name_hash == hash_val) {
                                        ingredients[ingredient_idx]._10 = true;
                                        found = true;
                                        break;
                                    }
                                }
                            }
                            if (found)
                                break;
                        }
                        if (found)
                            break;
                    }

                    al::ByamlIter actor_iter;
                    if (!ksys::act::InfoData::instance()->getActorIter(&actor_iter, uint_val))
                        continue;

                    actor_iter.tryGetStringByKey(&string_val, "name");
                    cook_item.actor_name = string_val;
                    cookCalcPotencyBoost(ingredients, cook_item);
                    if (ksys::act::InfoData::instance()->hasTag(cook_item.actor_name.cstr(),
                                                                ksys::act::tags::CookFailure)) {
                        goto COOK_FAILURE;
                    }

                    cookCalcBoost(ingredients, cook_item, &boost_arg);
                    cookCalcSpiceBoost(ingredients, cook_item);

                    if (recipe_iter.tryGetIntByKey(&int_val, "HB"))
                        cook_item.life_recover += (f32)int_val;

                    if (recipe_iter.tryGetIntByKey(&int_val, "TB")) {
                        if (cook_item.effect_time > 0) {
                            cook_item.effect_time += int_val;
                        }
                    }

                    cookAdjustItem(cook_item);

                    cookCalcItemPrice(ingredients, cook_item);
                    return true;
                }
            }
        }
    } else {
        goto BAD_RECIPE;
    }

    return true;
}

void CookingMgr::cookAdjustItem(CookItem& cook_item) const {
    cook_item.life_recover = (f32)(s32)cook_item.life_recover;

    const f32 life_recover_max = (f32)mCookingEffectEntries[(int)CookEffectId::LifeRecover].ma;
    cook_item.life_recover = sead::Mathf::clamp(cook_item.life_recover, 0.0f, life_recover_max);

    if (cook_item.effect_id != CookEffectId::None) {
        if (cook_item.stamina_recover > 0.0f && cook_item.stamina_recover < 1.0f)
            cook_item.stamina_recover = 1.0f;

        const s32 stamina_recover_max = mCookingEffectEntries[(int)cook_item.effect_id].ma;
        cook_item.stamina_recover =
            (f32)sead::Mathi::clampMax((s32)cook_item.stamina_recover, stamina_recover_max);

        if (cook_item.effect_id == CookEffectId::GutsRecover)
            cook_item.stamina_recover *= 200;

        if (cook_item.effect_id == CookEffectId::LifeMaxUp) {
            if ((s32)cook_item.stamina_recover % 4 != 0) {
                // Round up to whole heart.
                cook_item.stamina_recover = (f32)(((s32)cook_item.stamina_recover + 4) & ~3u);
            }

            if (cook_item.stamina_recover < 4.0f)
                cook_item.stamina_recover = 4.0f;

            cook_item.life_recover = cook_item.stamina_recover;
        }
    }

    cook_item.effect_time = sead::Mathi::clamp(cook_item.effect_time, 0, 1800);
}

void CookingMgr::prepareCookArg(
    CookArg& arg, const sead::SafeArray<sead::FixedSafeString<64>, NumIngredientsMax>& item_names,
    int num_items, CookItem& cook_item) const {
    for (int i = 0; i < NumIngredientsMax; i++) {
        arg.ingredients[i].name = "";
        arg.ingredients[i].count = 0;
    }

    arg.ingredients[0].count = 1;
    arg.ingredients[0].name = item_names[0];

    for (int i = 1; i < num_items; i++) {
        const auto& item_name = item_names[i];
        bool found_name = false;
        int j;
        for (j = 0; j < NumIngredientsMax; j++) {
            if (arg.ingredients[j].name == item_name) {
                arg.ingredients[j].count++;
                found_name = true;
                break;
            }
            if (arg.ingredients[j].name.isEmpty())
                break;
        }

        if (!found_name) {
            arg.ingredients[j].name = item_name;
            arg.ingredients[j].count = 1;
        }
    }

    cook_item.reset();

    for (int i = 0; i < NumIngredientsMax && i < num_items; i++) {
        cook_item.ingredients[i] = item_names[i];
    }
}

bool CookingMgr::cookWithItems(const sead::SafeString& item1, const sead::SafeString& item2,
                               const sead::SafeString& item3, const sead::SafeString& item4,
                               const sead::SafeString& item5, CookItem& cook_item,
                               const CookingMgr::BoostArg& boost_arg) {
    CookArg arg;
    sead::SafeArray<sead::FixedSafeString<64>, NumIngredientsMax> item_names;

    int num_items = 0;

    if (!item1.isEmpty()) {
        item_names[num_items].copy(item1);
        num_items++;
    }

    if (!item2.isEmpty()) {
        item_names[num_items].copy(item2);
        num_items++;
    }

    if (!item3.isEmpty()) {
        item_names[num_items].copy(item3);
        num_items++;
    }

    if (!item4.isEmpty()) {
        item_names[num_items].copy(item4);
        num_items++;
    }

    if (!item5.isEmpty()) {
        item_names[num_items].copy(item5);
        num_items++;
    }

    if (num_items > 0) {
        prepareCookArg(arg, item_names, num_items, cook_item);
        if (cook(arg, cook_item, boost_arg))
            return true;
    }
    return false;
}

void CookingMgr::setCookItem(const CookItem& from) {
    from.copy(mCookItem);
}

void CookingMgr::resetCookItem() {
    mCookItem.reset();
}

void CookingMgr::getCookItem(CookItem& to) const {
    mCookItem.copy(to);
}

}  // namespace uking
