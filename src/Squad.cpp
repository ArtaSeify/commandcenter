#include "Squad.h"
#include "CCBot.h"
#include "Util.h"

using namespace CC;

Squad::Squad(CCBot & bot)
    : m_bot(bot)
    , m_lastRetreatSwitch(0)
    , m_lastRetreatSwitchVal(false)
    , m_priority(0)
    , m_name("Default")
    , m_meleeManager(bot)
    , m_rangedManager(bot)
    , m_regroupingFrames(0)
{

}

Squad::Squad(const std::string & name, const SquadOrder & order, size_t priority, CCBot & bot)
    : m_bot(bot)
    , m_name(name)
    , m_order(order)
    , m_lastRetreatSwitch(0)
    , m_lastRetreatSwitchVal(false)
    , m_priority(priority)
    , m_meleeManager(bot)
    , m_rangedManager(bot)
    , m_regroupingFrames(0)
{
}

void Squad::onFrame()
{
    // update all necessary unit information within this squad
    updateUnits();

    // we are currently regrouping
    if (m_regroupingFrames > 0)
    {
        m_regroupingFrames--;
        return;
    }
    // determine whether or not we should regroup
    bool needToRegroup = needsToRegroup();
    
    // if we do need to regroup, do it
    if (needToRegroup)
    {
        m_regroupingFrames = 80;
        
        CCPosition regroupPosition = calcRegroupPosition();

        m_bot.Map().drawCircle(regroupPosition, 3, CCColor(255, 0, 255));

        m_meleeManager.regroup(regroupPosition);
        m_rangedManager.regroup(regroupPosition);
    }
    else // otherwise, execute micro
    {
        m_meleeManager.execute(m_order);
        m_rangedManager.execute(m_order);

        //_detectorManager.setUnitClosestToEnemy(unitClosestToEnemy());
        //_detectorManager.execute(_order);
    }
}

bool Squad::isEmpty() const
{
    return m_units.empty();
}

size_t Squad::getPriority() const
{
    return m_priority;
}

void Squad::setPriority(const size_t & priority)
{
    m_priority = priority;
}

void Squad::updateUnits()
{
    setAllUnits();
    setNearEnemyUnits();
    addUnitsToMicroManagers();
}

void Squad::setAllUnits()
{
    // clean up the _units vector just in case one of them died
    std::set<Unit> goodUnits;
    for (auto unit : m_units)
    {
        if (!unit.isValid()) { continue; }
        if (unit.isBeingConstructed()) { continue; }
        if (!unit.isAlive()) { continue; }
        
        goodUnits.insert(unit);
    }

    m_units = goodUnits;
}

void Squad::setNearEnemyUnits()
{
    m_nearEnemy.clear();
    for (auto unit : m_units)
    {
        m_nearEnemy[unit] = isUnitNearEnemy(unit);

        CCColor color = m_nearEnemy[unit] ? m_bot.Config().ColorUnitNearEnemy : m_bot.Config().ColorUnitNotNearEnemy;
        //m_bot.Map().drawCircleAroundUnit(unitTag, color);
    }
}

void Squad::addUnitsToMicroManagers()
{
    std::vector<Unit> meleeUnits;
    std::vector<Unit> rangedUnits;
    std::vector<Unit> detectorUnits;
    std::vector<Unit> transportUnits;
    std::vector<Unit> tankUnits;

    // add _units to micro managers
    for (auto unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit in addUnitsToMicroManagers()");

        if (unit.getType().isTank())
        {
            tankUnits.push_back(unit);
        }
        // TODO: detectors
        else if (unit.getType().isDetector() && !unit.getType().isBuilding())
        {
            detectorUnits.push_back(unit);
        }
        // select ranged _units
        else if (unit.getType().getAttackRange() >= 1.5f)
        {
            rangedUnits.push_back(unit);
        }
        // select melee _units
        else if (unit.getType().getAttackRange() < 1.5f)
        {
            meleeUnits.push_back(unit);
        }
    }

    m_meleeManager.setUnits(meleeUnits);
    m_rangedManager.setUnits(rangedUnits);
    //m_detectorManager.setUnits(detectorUnits);
    //m_tankManager.setUnits(tankUnits);
}

bool Squad::needsToRegroup() const
{
    // dont need to regroup idle squad
    if (m_name == "Idle")
    {
        return false;
    }

    // one of our units is near the enemy, so don't regroup
    for (const auto& unit : m_units)
    {
        if (unit.getType().isCombatUnit() && m_nearEnemy.at(unit))
        {
            return false;
        }
    }

    float minDist = std::numeric_limits<float>::max();
    Unit minDistUnit;

    // regroup if there is a big gap between the units attacking. 
    for (auto & unit : m_units)
    {
        float dist = Util::Dist(m_order.getPosition(), unit.getPosition());
        if (dist < minDist)
        {
            minDist = dist;
            minDistUnit = unit;
        }
    }

    for (auto & unit : m_units)
    {
        float dist = Util::Dist(unit.getPosition(), minDistUnit.getPosition());
        // prevents regrouping because of newly made units
        if (dist > 30)
        {
            continue;
        }
        if (dist > 15)
        {
            return true;
        }
    }

    return false;
}

void Squad::setSquadOrder(const SquadOrder & so)
{
    m_order = so;
}

bool Squad::containsUnit(const Unit & unit) const
{
    return std::find(m_units.begin(), m_units.end(), unit) != m_units.end();
}

void Squad::clear()
{
    for (auto unit : getUnits())
    {
        BOT_ASSERT(unit.isValid(), "null unit in squad clear");

        if (unit.getType().isWorker())
        {
            m_bot.Workers().finishedWithWorker(unit);
        }
    }

    m_units.clear();
}

bool Squad::isUnitNearEnemy(const Unit & unit) const
{
    BOT_ASSERT(unit.isValid(), "null unit in squad");

    for (auto & u : m_bot.GetUnits())
    {
        if ((u.getPlayer() == Players::Enemy) && (Util::Dist(unit, u) < 20))
        {
            return true;
        }
    }

    return false;
}

CCPosition Squad::calcCenter() const
{
    if (m_units.empty())
    {
        return CCPosition(0, 0);
    }

    CCPosition sum(0, 0);
    for (auto unit: m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit in squad calcCenter");
        sum += unit.getPosition();
    }

    return CCPosition(sum.x / m_units.size(), sum.y / m_units.size());
}

CCPosition Squad::calcRegroupPosition() const
{
    CCPosition regroup(0, 0);

    float minDist = std::numeric_limits<float>::max();

    //regroup = calcCenter();

    for (auto unit : m_units)
    {
        //if (!m_nearEnemy.at(unit))
        //{
            float dist = Util::Dist(m_order.getPosition(), unit.getPosition());
            if (dist < minDist)
            {
                minDist = dist;
                regroup = unit.getPosition();
            }
        //}
    }

    if (regroup.x == 0.0f && regroup.y == 0.0f)
    {
        return m_bot.GetStartLocation();
    }
    else
    {
        return regroup;
    }
}

Unit Squad::unitClosestToEnemy() const
{
    Unit closest;
    float closestDist = std::numeric_limits<float>::max();

    for (auto & unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit");

        // the distance to the order position
        int dist = m_bot.Map().getGroundDistance(unit.getPosition(), m_order.getPosition());

        if (dist != -1 && (!closest.isValid() || dist < closestDist))
        {
            closest = unit;
            closestDist = (float)dist;
        }
    }

    return closest;
}

int Squad::squadUnitsNear(const CCPosition & p) const
{
    int numUnits = 0;

    for (auto unit : m_units)
    {
        BOT_ASSERT(unit.isValid(), "null unit");

        if (Util::Dist(unit, p) < 20.0f)
        {
            numUnits++;
        }
    }

    return numUnits;
}

const std::set<Unit> & Squad::getUnits() const
{
    return m_units;
}

const SquadOrder & Squad::getSquadOrder()	const
{
    return m_order;
}

void Squad::addUnit(const Unit & unit)
{
    m_units.insert(unit);
}

void Squad::removeUnit(const Unit & unit)
{
    m_units.erase(unit);
}

const std::string & Squad::getName() const
{
    return m_name;
}