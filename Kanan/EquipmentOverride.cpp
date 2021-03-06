#include <imgui.h>

#include <Scan.hpp>

#include "Kanan.hpp"
#include "Log.hpp"
#include "EquipmentOverride.hpp"

using namespace std;

namespace kanan {
    static EquipmentOverride* g_equipmentOverride{ nullptr };

    int convertInventoryIDToEquipmentSlot(int inventoryID) {
        static auto fn = (int(__cdecl*)(int))scan("client.exe", "55 8B EC 8B 45 08 83 C0 ? 83 F8 ? 77 ? 0F B6 80 8C 64 66 01").value_or(0);
        static bool logged{ false };

        if (!logged) {
            log("[EquipmentOverride] convertInventoryIDToEquipmentSlot %p", fn);
            logged = true;
        }

        return (fn != nullptr) ? fn(inventoryID) : 0;
    }

    EquipmentOverride::EquipmentOverride()
        : m_equipmentOverrides{},
        m_setEquipmentInfoHook{}
    {
        log("[EquipmentOverride] Entering constructor");

        g_equipmentOverride = this;

        // Set the names for the equipment slots.
        m_equipmentOverrides[1].name = "Torso\\Armor\\Shirt";
        m_equipmentOverrides[2].name = "Head\\Helmet\\Hat";
        m_equipmentOverrides[4].name = "Hands\\Gauntlets\\Gloves";
        m_equipmentOverrides[5].name = "Feet\\Boots\\Shoes";
        m_equipmentOverrides[7].name = "Hair";
        m_equipmentOverrides[8].name = "Back\\Wings\\Robe";
        m_equipmentOverrides[10].name = "Weapon 1";
        m_equipmentOverrides[11].name = "Weapon 2";
        m_equipmentOverrides[12].name = "Arrow\\Shield 1";
        m_equipmentOverrides[13].name = "Arrow\\Shield 2";
        m_equipmentOverrides[15].name = "Tail";
        m_equipmentOverrides[16].name = "Accessory 1";
        m_equipmentOverrides[17].name = "Accessory 2";

        auto address = scan("client.exe", "55 8B EC 6A ? 68 ? ? ? ? 64 A1 ? ? ? ? 50 83 EC ? 53 56 57 A1 ? ? ? ? 33 C5 50 8D 45 F4 64 A3 ? ? ? ? 8B F1 8B 4E 04 33 FF");

        if (address) {
            log("[EquipmentOverride] Found address of setEquipmentInfo %p", *address);

            m_setEquipmentInfoHook = make_unique<FunctionHook>(*address, (uintptr_t)&EquipmentOverride::hookedSetEquipmentInfo);
        }

        log("[EquipmentOverride] Leaving constructor");
    }

    EquipmentOverride::~EquipmentOverride() {
        m_setEquipmentInfoHook->remove();
        g_equipmentOverride = nullptr;
    }

    void EquipmentOverride::onUI() {
        if (ImGui::CollapsingHeader("Equipment Override")) {
            ImGui::TextWrapped("After enabling an override you must re-equip an item in that slot to see the changes. "
                "NOTE: For the Hair slot, you need to change your hair in the dressing room to see the changes.");
            ImGui::Spacing();
            ImGui::Text("Special thanks to Rydian!");
            ImGui::Spacing();

            for (auto& overrideInfo : m_equipmentOverrides) {
                if (overrideInfo.name.empty()) {
                    continue;
                }

                if (ImGui::TreeNode(&overrideInfo, "%s", overrideInfo.name.c_str())) {
                    ImGui::Checkbox("Enable color override", &overrideInfo.isOverridingColor);
                    ImGui::ColorEdit4("Color 1", overrideInfo.color1.data(), ImGuiColorEditFlags_HEX);
                    ImGui::ColorEdit4("Color 2", overrideInfo.color2.data(), ImGuiColorEditFlags_HEX);
                    ImGui::ColorEdit4("Color 3", overrideInfo.color3.data(), ImGuiColorEditFlags_HEX);
                    ImGui::Checkbox("Enable item override", &overrideInfo.isOverridingItem);
                    ImGui::InputInt("Item ID", (int*)&overrideInfo.itemID);
                    ImGui::TreePop();
                }
            }
        }
    }

    void EquipmentOverride::hookedSetEquipmentInfo(CEquipment* equipment, uint32_t EDX, int inventoryID, int itemID, int a4, int a5, uint32_t* color, int a7, int * a8, int a9, int a10, int * a11) {
        auto orig = (decltype(hookedSetEquipmentInfo)*)g_equipmentOverride->m_setEquipmentInfoHook->getOriginal();

        // Filter out other characters.
        auto game = g_kanan->getGame();
        auto localCharacter = game->getLocalCharacter();

        if (localCharacter == nullptr || equipment != localCharacter->equipment) {
            return orig(equipment, EDX, inventoryID, itemID, a4, a5, color, a7, a8, a9, a10, a11);
        }

        // Filter out inventoryIDs.
        auto equipmentSlot = convertInventoryIDToEquipmentSlot(inventoryID);

        if (equipmentSlot < 0 || equipmentSlot >= g_equipmentOverride->m_equipmentOverrides.size()) {
            return orig(equipment, EDX, inventoryID, itemID, a4, a5, color, a7, a8, a9, a10, a11);
        }

        auto& overrideInfo = g_equipmentOverride->m_equipmentOverrides[equipmentSlot];

        if (overrideInfo.isOverridingColor) {
            // Convert float color to int.
            auto convertFloatColorToInt = [](array<float, 4>& color) {
                auto r = (uint8_t)(255 * color[0]);
                auto g = (uint8_t)(255 * color[1]);
                auto b = (uint8_t)(255 * color[2]);
                auto a = (uint8_t)(255 * color[3]);
                return (a << 24) + (r << 16) + (g << 8) + b;
            };

            color[0] = convertFloatColorToInt(overrideInfo.color1);
            color[1] = convertFloatColorToInt(overrideInfo.color2);
            color[2] = convertFloatColorToInt(overrideInfo.color3);

            log("[EquipmentOverride] Color overwritten!");
        }

        if (overrideInfo.isOverridingItem) {
            itemID = overrideInfo.itemID;

            log("[EquipmentOverride] Item overwritten!");
        }

        return orig(equipment, EDX, inventoryID, itemID, a4, a5, color, a7, a8, a9, a10, a11);
    }
}