"""The real `AgentDocs/Tasks.json`: wordings agents used for machinery the engine already has reach its route.

Each case is how an agent described a task while writing that machinery itself. When a game scan finds an engine
tool a game did not use, add the game's wording here, with the file or recipe its route should print."""

import unittest

from Tools.agentdocs.query import answer
from Tools.manifest import ENGINE_ROOT

TOP_ROUTES = 3
CASES = {
    'enemies list': 'AgentDocs/Recipes/Pool.md',
    'keep spawned things in a vector by index': 'AgentDocs/Recipes/Pool.md',
    'sweep a moving prop past its own collider': 'Core/Physics/Collision/CollisionWorld.h',
    'query ignores its own collider': 'Core/Physics/Collision/CollisionWorld.h',
    'push a crate': 'Core/Physics/Collision/CollisionWorld.h',
    'player slides along walls': 'Core/Physics/Collision/MoveAndSlide.h',
    'sweep each axis separately so the character slides': 'Core/Physics/Collision/MoveAndSlide.h',
    'walk through a doorway without exact aim': 'Core/Physics/Collision/MoveAndSlide.h',
    'platformer on the ground': 'Core/Physics/Collision/MoveAndSlide.h',
    'footstep sound on animation frame': 'AgentDocs/Recipes/Animator.md',
    'muzzle flash on a clip frame': 'AgentDocs/Recipes/Animator.md',
    'short-lived effects with age that expire': 'AgentDocs/Recipes/PopupEffects.md',
    'spent casings that fade': 'AgentDocs/Recipes/PopupEffects.md',
    'pause screen resume button': 'AgentDocs/Recipes/UiMenu.md',
    'dialog over the game': 'AgentDocs/Recipes/UiMenu.md',
    'tile map': 'Core/Utilities/Geometry/Grid.h',
    'random ambience weighted': 'Core/Utilities/Random/WeightedPick.h',
    'boss attack sequence': 'AgentDocs/Recipes/Timeline.md',
}


def routes(text: str) -> list[str]:
    """The task routes of a words answer, each as its title and its files line."""
    lines = text.splitlines()
    if not lines or lines[0] != 'Tasks (files | recipe | concept docs):':
        return []
    found = []
    for title, files in zip(lines[1::2], lines[2::2]):
        if not title.startswith('- '):
            break
        found.append(f'{title}\n{files}')
    return found


class RealRoutesTest(unittest.TestCase):
    def test_wordings_reach_their_route(self):
        for query, expected in CASES.items():
            with self.subTest(query=query):
                text, found = answer(ENGINE_ROOT, query)
                self.assertTrue(found, text)
                best = routes(text)[:TOP_ROUTES]
                self.assertTrue(any(expected in route for route in best),
                                f'{expected} is not in the first {TOP_ROUTES} routes:\n' + '\n'.join(best))


if __name__ == '__main__':
    unittest.main()
