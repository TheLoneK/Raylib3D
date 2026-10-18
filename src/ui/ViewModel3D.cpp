#include "ui/ViewModel3D.h"
#include "assets/AssetLibrary.h"
#include "assets/AssetSlots.h"
#include "core/MathUtils.h"
#include "player/Player.h"
#include "raymath.h"

ViewModel3D::~ViewModel3D()
{
    if (m_target.id != 0) UnloadRenderTexture(m_target);
}

bool ViewModel3D::Draw(const Player &player, AssetLibrary &library)
{
    const Inventory &inventory = player.GetInventory();
    const Weapon *weapon = inventory.GetActiveWeapon();
    if ((weapon == nullptr) || !player.IsAlive()) return false;

    const char *slot = GetWeaponSlotId(weapon->GetType());
    if (!library.HasModel(slot)) return false;
    const WeaponDef &def = weapon->GetDef();

    int width = GetScreenWidth();
    int height = GetScreenHeight();
    if ((m_target.id == 0) || (m_target.texture.width != width) || (m_target.texture.height != height))
    {
        if (m_target.id != 0) UnloadRenderTexture(m_target);
        m_target = LoadRenderTexture(width, height);
    }

    // Same motion as the 2D viewmodel: switching lowers, reloading dips, walking bobs, firing kicks
    float time = static_cast<float>(GetTime());
    float aim = player.GetAimAmount();
    float aimEased = aim*aim*(3.0f - 2.0f*aim);
    float sway = 1.0f - aimEased*0.85f;
    float walk = player.IsGrounded() ? Clamp(player.GetHorizontalSpeed()/20.0f, 0.0f, 1.0f) : 0.0f;
    float reloadDip = sinf(weapon->GetReloadProgress()*PI);
    float recoil = weapon->GetRecoil();
    float shake = (def.shotKind == ShotKind::Melee) && (weapon->GetMuzzleFlash() > 0.0f) ? 0.006f : 0.0f;

    Vector3 position = Vector3Lerp(ViewModelLayout::HipPosition, ViewModelLayout::AimPosition, aimEased);
    position.x += sinf(time*8.0f)*0.012f*walk*sway + MathUtils::RandomFloat(-1.0f, 1.0f)*shake;
    position.y += -fabsf(cosf(time*8.0f))*0.01f*walk*sway - inventory.GetLowerAmount()*0.35f - reloadDip*0.12f + MathUtils::RandomFloat(-1.0f, 1.0f)*shake;
    position.z += recoil*0.06f + weapon->GetDrawAmount()*0.06f;
    float pitch = recoil*8.0f - reloadDip*25.0f;

    AssetDrawParams params;
    params.placement = MatrixMultiply(MatrixRotateX(pitch*DEG2RAD), MatrixTranslate(position.x, position.y, position.z));
    params.fitSize = ViewModelLayout::WeaponLength;
    params.fitMode = FitMode::Length;
    params.time = time;

    Camera3D camera = ViewModelLayout::GetCamera();
    BeginTextureMode(m_target);
        ClearBackground(BLANK);
        library.SetView(camera.position, BLACK, 1000.0f, 2000.0f);     // No fog this close
        BeginMode3D(camera);
            library.DrawSlot(slot, params);

            // Muzzle flash at the front of the model
            const AssetBinding *binding = library.GetManifest().Find(slot);
            bool shoots = (def.shotKind != ShotKind::Melee) && (def.fireMode != FireMode::Charge);
            if (shoots && (binding != nullptr) && (weapon->GetMuzzleFlash() > 0.0f))
            {
                BoundingBox bounds = AssetLibrary::GetPlacedBounds(library.GetModel(binding->model), *binding, ViewModelLayout::WeaponLength, FitMode::Length);
                Vector3 muzzle = Vector3Transform(Vector3{ (bounds.min.x + bounds.max.x)*0.5f, (bounds.min.y + bounds.max.y)*0.5f, bounds.min.z }, params.placement);
                float flash = weapon->GetMuzzleFlash()/Weapon::MuzzleFlashTime;
                DrawSphere(muzzle, 0.02f + 0.03f*flash, Fade(Color{ 255, 200, 80, 255 }, 0.8f));
                DrawSphere(muzzle, 0.01f + 0.012f*flash, Color{ 255, 250, 220, 255 });
            }
        EndMode3D();
    EndTextureMode();

    // Render textures are stored upside down
    DrawTextureRec(m_target.texture, Rectangle{ 0.0f, 0.0f, static_cast<float>(width), -static_cast<float>(height) }, Vector2{ 0.0f, 0.0f }, WHITE);
    return true;
}
