/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vk_api.h"
#include <stddef.h>
_Static_assert(MKWVK_API_VERSION == 20, "head camera callback requires ABI 20");
_Static_assert(sizeof(MkwVkCameraObservation) == 176, "camera observation wire layout");
void vr_camera_c_fixture(void (*receive)(const MkwVkCameraObservation *), size_t sizes[3]) {
    sizes[0]=sizeof(MkwVkCameraObservation);sizes[1]=offsetof(MkwVkCameraObservation,view);sizes[2]=offsetof(MkwVkCameraObservation,projection);
    MkwVkCameraObservation sample={0};sample.size=sizeof sample;sample.flags=MKWVK_CAMERA_PRIMARY;
    sample.camera_id=123;sample.sequence=456;sample.eye[0]=1.25;sample.view[3]=-1.25f;receive(&sample);
}