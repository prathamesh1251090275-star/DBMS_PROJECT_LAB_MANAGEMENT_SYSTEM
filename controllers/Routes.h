// Each controller file exposes one function that registers its URLs with Drogon.
#pragma once
void registerAuthRoutes();
void registerEquipmentRoutes();
void registerLookupRoutes();   // labs, categories, users
void registerIssueRoutes();
void registerMaintenanceRoutes();
void registerReportRoutes();   // dashboard + reports
