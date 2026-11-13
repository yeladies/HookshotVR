"""Build an ordered same-state eye-work review from observed dependencies.

This is a planning component, never a replay authorization or a UI classifier.
"""


def plan(events, selected, dependencies, imported_images, imported_buffers, descriptions):
    dependent = set()
    mismatches = []
    ordered = sorted(selected)
    for index in range(len(events)):
        event = events[index]
        if event['seed'] or dependencies[index] & dependent:
            dependent.add(index)
        if index in selected and event['dependent'] != (index in dependent):
            mismatches.append(index)
    eye_work = [i for i in ordered if i in dependent]
    shared_work = [i for i in ordered if i not in dependent]
    images = sorted({raw[0] for i in eye_work for raw in events[i]['writes']})
    buffers = sorted({tuple(raw[:3]) for i in eye_work for raw in events[i].get('buffer_writes', [])})
    # Roots may be uploaded constants, previous-frame history or unobserved work.
    # None is assumed eye-independent merely because its producer is absent.
    roots = sorted({image for i in ordered for image in imported_images[i]})
    buffer_roots = sorted({raw for i in ordered for raw in imported_buffers[i]})
    blockers = ['capture coverage is incomplete', 'image subresources, aliases and partial writes require native validation',
                'eye-dependent visibility and previous-frame histories are unproven',
                'HUD/text must be isolated before world-lens composition',
                'GPU-only swapchain delivery and lifetime synchronization are not implemented']
    if mismatches:
        blockers.append('recorded camera dependency flags disagree with the observed graph')
    if roots or buffer_roots:
        blockers.append('imported resources require explicit shared-input or per-eye-history classification')
    if any(events[i]['opaque_buffers'] for i in ordered):
        blockers.append('opaque buffer access exists in the presented slice')
    if any(i not in descriptions or descriptions[i]['sparse'] or not descriptions[i]['allocation_bytes'] for i in images):
        blockers.append('private image allocation inventory is missing or unsupported')
    return dict(schema=1, replay_ready=False, same_simulation_state_required=True,
                simulation_updates_per_pair=1, atomic_pair_submission_required=True,
                shared_work_review_indices=shared_work, per_eye_work_review_indices=eye_work,
                dependency_flag_mismatch_indices=mismatches,
                per_eye_image_ids=images, per_eye_buffer_ranges=buffers,
                imported_image_review_ids=roots, imported_buffer_review_ranges=buffer_roots,
                # An unseeded overlay can consume the scene as an attachment. It
                # inherits dependency but is not thereby a world-space draw.
                unseeded_graphics_review_indices=[i for i in eye_work if events[i]['kind'] == 0 and not events[i]['seed']],
                hud_separation_verified=False, blockers=blockers,
                work=[dict(index=i, kind=events[i]['kind'], camera_seed=events[i]['seed'],
                           observed_eye_dependent=i in dependent, parents=sorted(dependencies[i]),
                           image_outputs=events[i]['writes'], buffer_outputs=events[i].get('buffer_writes', []))
                      for i in ordered])
