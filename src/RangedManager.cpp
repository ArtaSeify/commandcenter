#include "RangedManager.h"
#include "Util.h"
#include "CCBot.h"

using namespace CC;

RangedManager::RangedManager(CCBot & bot)
    : MicroManager(bot)
{

}

void RangedManager::executeMicro(const std::vector<Unit> & targets)
{
    assignTargets(targets);
}

void RangedManager::assignTargets(const std::vector<Unit> & targets)
{
    const std::vector<Unit> & rangedUnits = getUnits();

    // figure out targets
    std::vector<Unit> rangedUnitTargets;
    for (auto target : targets)
    {
        if (!target.isValid()) { continue; }
        if (target.getType().isEgg()) { continue; }
        if (target.getType().isLarva()) { continue; }
        if (!m_bot.Map().isVisible(target.getTilePosition().x, target.getTilePosition().y)) { continue; }

        rangedUnitTargets.push_back(target);
    }

    // for each meleeUnit
    for (auto rangedUnit : rangedUnits)
    {
        BOT_ASSERT(rangedUnit.isValid(), "ranged unit is null");

        // if the order is to attack or defend
        if (order.getType() == SquadOrderTypes::Attack || order.getType() == SquadOrderTypes::Defend)
        {
            if (!rangedUnitTargets.empty())
            {
                // find the best target for this meleeUnit
                Unit target = getTarget(rangedUnit, rangedUnitTargets);

                // attack it
                if (m_bot.Config().KiteWithRangedUnits)
                {
                    // TODO: implement kiting
                    rangedUnit.attackUnit(target);
                }
                else
                {
                    rangedUnit.attackUnit(target);
                }
            }
            // if there are no targets
            else
            {
                // if we're not near the order position
                if (Util::Dist(rangedUnit, order.getPosition()) > 4)
                {
                    // don't spam move command
                    if (rangedUnit.getUnitPtr()->orders.size() > 0 && CCPosition(rangedUnit.getUnitPtr()->orders[0].target_pos) == order.getPosition())
                    {
                        continue;
                    }
                    // move to it
                    rangedUnit.move(order.getPosition());
                }
            }
        }

        if (m_bot.Config().DrawUnitTargetInfo)
        {
            // TODO: draw the line to the unit's target
        }
    }
}

// get a target for the ranged unit to attack
// TODO: this is the melee targeting code, replace it with something better for ranged units
Unit RangedManager::getTarget(const Unit & rangedUnit, const std::vector<Unit> & targets)
{
    BOT_ASSERT(rangedUnit.isValid(), "null ranged unit in getTarget");

    int highPriority = 0;
    double closestDist = std::numeric_limits<double>::max();
    Unit closestTarget;

    // for each target possiblity
    for (auto & targetUnit : targets)
    {
        BOT_ASSERT(targetUnit.isValid(), "null target unit in getTarget");

        int priority = getAttackPriority(rangedUnit, targetUnit);
        float distance = Util::Dist(rangedUnit, targetUnit);

        // if it's a higher priority, or it's closer, set it
        if (!closestTarget.isValid() || (priority > highPriority) || (priority == highPriority && distance < closestDist))
        {
            closestDist = distance;
            highPriority = priority;
            closestTarget = targetUnit;
        }
    }

    return closestTarget;
}

// get the attack priority of a type in relation to a zergling
int RangedManager::getAttackPriority(const Unit & attacker, const Unit & enemyUnit)
{
    BOT_ASSERT(enemyUnit.isValid(), "null unit in getAttackPriority");

    if (enemyUnit.getType().isCombatUnit())
    {
        // if our unit is strong against the enemy unit, we want it to attack that target
        const auto & unitInfo = m_bot.Observation()->GetUnitTypeData()[attacker.getType().getAPIUnitType()];
        if (unitInfo.weapons.size() > 0)
        {
            const auto & enemyUnitInfo = m_bot.Observation()->GetUnitTypeData()[enemyUnit.getType().getAPIUnitType()];
            const auto & bonusDamagesInfo = unitInfo.weapons[0].damage_bonus;
            for (const auto & bonusDamageInfo : bonusDamagesInfo)
            {
                if (std::find(enemyUnitInfo.attributes.begin(), enemyUnitInfo.attributes.end(), bonusDamageInfo.attribute) != enemyUnitInfo.attributes.end())
                {
                    return 15;
                }
            }
        }
        
        return 10;
    }

    if (enemyUnit.getType().isWorker())
    {
        return 9;
    }

    return 1;
}

