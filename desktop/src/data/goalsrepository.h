#pragma once

#include "domain/entities.h"

#include <QSqlDatabase>

#include <vector>

namespace SmTool::Data {

class GoalsRepository
{
public:
    explicit GoalsRepository(QSqlDatabase database);

    [[nodiscard]] std::vector<Domain::Goal> listGoals() const;
    [[nodiscard]] Domain::Goal getGoal(const QString &id) const;
    [[nodiscard]] QString createGoal(const Domain::Goal &goal, QString *errorMessage = nullptr) const;
    bool updateGoal(const Domain::Goal &goal, QString *errorMessage = nullptr) const;
    bool deleteGoal(const QString &id, QString *errorMessage = nullptr) const;
    bool setGoalEnabled(const QString &id, bool enabled, QString *errorMessage = nullptr) const;
    [[nodiscard]] std::vector<Domain::GoalBalanceItem> listBalanceItems(const QString &goalId) const;
    bool updateBalanceItems(const QString &goalId,
                            const QString &scopeType,
                            const std::vector<Domain::GoalBalanceItem> &items,
                            QString *errorMessage = nullptr) const;
    [[nodiscard]] bool scopeExists(const QString &scopeType, const QString &scopeId) const;

private:
    [[nodiscard]] QString scopeDisplayName(const QString &scopeType, const QString &scopeId) const;
    [[nodiscard]] QString metricTypeForScope(const QString &scopeType) const;
    [[nodiscard]] QString periodLabel(const QString &periodType, int periodValue) const;
    [[nodiscard]] QString buildSummary(const Domain::Goal &goal) const;

    QSqlDatabase database_;
};

} // namespace SmTool::Data
