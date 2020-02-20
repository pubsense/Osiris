#include <Windows.h>

#include "../Config.h"
#include "../Interfaces.h"
#include "../Memory.h"
#include "../SDK/ConVar.h"
#include "../SDK/Entity.h"
#include "../SDK/GlobalVars.h"
#include "../SDK/WeaponData.h"
#include "../SDK/WeaponId.h"
#include "Triggerbot.h"

void Triggerbot::run(UserCmd* cmd) noexcept
{
    const auto localPlayer = interfaces.entityList->getEntity(interfaces.engine->getLocalPlayer());
    if (!localPlayer || !localPlayer->isAlive() || localPlayer->nextAttack() > memory.globalVars->serverTime())
        return;

    const auto activeWeapon = localPlayer->getActiveWeapon();
    if (!activeWeapon || !activeWeapon->clip() || activeWeapon->nextPrimaryAttack() > memory.globalVars->serverTime())
        return;

    auto weaponIndex = getWeaponIndex(activeWeapon->itemDefinitionIndex2());
    if (!weaponIndex)
        return;

    if (!config.triggerbot[weaponIndex].enabled)
        weaponIndex = getWeaponClass(activeWeapon->itemDefinitionIndex2());

    if (!config.triggerbot[weaponIndex].enabled)
        weaponIndex = 0;

    if (!config.triggerbot[weaponIndex].enabled)
        return;

    static auto lastTime = 0.0f;

    if (const auto now{ memory.globalVars->realtime };
        (GetAsyncKeyState(config.triggerbot[weaponIndex].key) || !config.triggerbot[weaponIndex].onKey)
        && now - lastTime >= config.triggerbot[weaponIndex].shotDelay / 1000.0f) {

        const auto aimPunch = localPlayer->getAimPunch();
        const auto weaponData = activeWeapon->getWeaponData();

        const Vector viewAngles{ std::cos(degreesToRadians(cmd->viewangles.x + aimPunch.x)) * std::cos(degreesToRadians(cmd->viewangles.y + aimPunch.y)) * weaponData->range,
                                 std::cos(degreesToRadians(cmd->viewangles.x + aimPunch.x)) * std::sin(degreesToRadians(cmd->viewangles.y + aimPunch.y)) * weaponData->range,
                                -std::sin(degreesToRadians(cmd->viewangles.x + aimPunch.x)) * weaponData->range };
        Trace trace;
        interfaces.engineTrace->traceRay({ localPlayer->getEyePosition(), localPlayer->getEyePosition() + viewAngles }, 0x46004009, localPlayer, trace);
        if (trace.entity && trace.entity->getClientClass()->classId == ClassId::CSPlayer
            && (config.triggerbot[weaponIndex].friendlyFire
                || trace.entity->isEnemy())
            && !trace.entity->gunGameImmunity()
            && (!config.triggerbot[weaponIndex].hitgroup
                || trace.hitgroup == config.triggerbot[weaponIndex].hitgroup)
            && (config.triggerbot[weaponIndex].ignoreSmoke
                || !memory.lineGoesThroughSmoke(localPlayer->getEyePosition(), localPlayer->getEyePosition() + viewAngles, 1))
            && (config.triggerbot[weaponIndex].ignoreFlash
                || !localPlayer->flashDuration())
            && (!config.triggerbot[weaponIndex].scopedOnly
                || !activeWeapon->isSniperRifle()
                || localPlayer->isScoped())) {

            float damage = (activeWeapon->itemDefinitionIndex2() != WeaponId::Taser ? HitGroup::getDamageMultiplier(trace.hitgroup) : 1.0f) * activeWeapon->getWeaponData()->damage * powf(activeWeapon->getWeaponData()->rangeModifier, trace.fraction * weaponData->range / 500.0f);

            if (float armorRatio{ activeWeapon->getWeaponData()->armorRatio / 2.0f }; activeWeapon->itemDefinitionIndex2() != WeaponId::Taser && HitGroup::isArmored(trace.hitgroup, trace.entity->hasHelmet()))
                damage -= (trace.entity->armor() < damage * armorRatio / 2.0f ? trace.entity->armor() * 4.0f : damage) * (1.0f - armorRatio);

            if (damage >= (config.triggerbot[weaponIndex].killshot ? trace.entity->health() : config.triggerbot[weaponIndex].minDamage)) {
                if ((!config.triggerbot[weaponIndex].hitChance || (1.0f - (activeWeapon->getInaccuracy())) * 100.f >= hitchance)) {
                    cmd->buttons |= UserCmd::IN_ATTACK;
                    lastTime = 0.0f;
                }
            }
        } else {
            lastTime = now;
        }
    }
}
