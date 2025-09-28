#pragma once

#include <QString>

typedef struct RecipeProfile {
    double batchLiters = 10.0;
    double targetPh = 6.0;
    double fertAMlPerL = 1.5;
    double fertBMlPerL = 1.5;
    double temperature = 25.0;
    QString name;
} RecipeProfile;
