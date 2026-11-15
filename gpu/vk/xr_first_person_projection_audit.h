// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
namespace mkwvk {
struct FirstPersonProjectionLookup {
    // 1: no retained present; 2: projection rejected; 3: packet retrieved.
    int status=0,camera_status=0;
    int64_t oldest_present=-1,newest_present=-1;
    uint32_t retained=0;
};
// Diagnostic only: observation consistency is not a producer-owned frame ID.
struct FirstPersonProjectionEvidence {
    FirstPersonProjectionLookup lookup{};
    bool available=false,center_matches=false,depth_exact=false,fov_matches=false,synthetic=false;
    uint64_t sequence=0,origin=0;
    int64_t predicted_time=0,observed_ns=0;
    double effective_near=0,effective_far=0,eye_separation_world=0;
};
FirstPersonProjectionEvidence xr_first_person_native_projection_evidence(int64_t game_present,const float view[12],const float projection[16]);
}
