# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

"""
./blender.bin --background -noaudio --factory-startup --python tests/python/bl_constraints.py -- --testdir /path/to/lib/tests/constraints
"""

import pathlib
import sys
import unittest

import bpy
from mathutils import Matrix


class AbstractConstraintTests(unittest.TestCase):
    """Useful functionality for constraint tests."""

    layer_collection = ''  # When set, this layer collection will be enabled.

    def setUp(self):
        bpy.ops.wm.open_mainfile(filepath=str(args.testdir / "constraints.blend"))

        # This allows developers to disable layer collections and declutter the
        # 3D Viewport while working in constraints.blend, without influencing
        # the actual unit tests themselves.
        if self.layer_collection:
            top_collection = bpy.context.view_layer.layer_collection
            collection = top_collection.children[self.layer_collection]
            collection.exclude = False

    def assert_matrix(self, actual_matrix, expect_matrix, object_name: str, places=6, delta=None):
        """Asserts that the matrices almost equal."""
        self.assertEqual(len(actual_matrix), 4, 'Expected a 4x4 matrix')

        # TODO(Sybren): decompose the matrices and compare loc, rot, and scale separately.
        # That'll probably improve readability & understandability of test failures.
        for row, (act_row, exp_row) in enumerate(zip(actual_matrix, expect_matrix)):
            for col, (actual, expect) in enumerate(zip(act_row, exp_row)):
                self.assertAlmostEqual(
                    actual, expect, places=places, delta=delta,
                    msg=f'Matrix of object {object_name!r} failed: {actual} != {expect} at element [{row}][{col}]')

    def matrix(self, object_name: str) -> Matrix:
        """Return the evaluated world matrix."""
        depsgraph = bpy.context.view_layer.depsgraph
        depsgraph.update()
        ob_orig = bpy.context.scene.objects[object_name]
        ob_eval = ob_orig.evaluated_get(depsgraph)
        return ob_eval.matrix_world

    def matrix_test(self, object_name: str, expect: Matrix):
        """Assert that the object's world matrix is as expected."""
        actual = self.matrix(object_name)
        self.assert_matrix(actual, expect, object_name)

    def constraint_context(self, constraint_name: str, owner_name: str='') -> dict:
        """Return a context suitable for calling constraint operators.

        Assumes the owner is called "{constraint_name}.owner" if owner_name=''.
        """
        owner = bpy.context.scene.objects[owner_name or f'{constraint_name}.owner']
        constraint = owner.constraints[constraint_name]
        context = {
            **bpy.context.copy(),
            'object': owner,
            'active_object': owner,
            'constraint': constraint,
        }
        return context


class ChildOfTest(AbstractConstraintTests):
    layer_collection = 'Child Of'

    def test_object_simple_parent(self):
        """Child Of: simple evaluation of object parent."""
        initial_matrix = Matrix((
            (0.5872668623924255, -0.3642929494380951, 0.29567837715148926, 1.0886117219924927),
            (0.31689348816871643, 0.7095895409584045, 0.05480116978287697, 2.178966999053955),
            (-0.21244174242019653, 0.06738340109586716, 0.8475662469863892, 3.2520291805267334),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Child Of.object.owner', initial_matrix)

        context = self.constraint_context('Child Of', owner_name='Child Of.object.owner')
        bpy.ops.constraint.childof_set_inverse(context, constraint='Child Of')
        self.matrix_test('Child Of.object.owner', Matrix((
            (0.9992385506629944, 0.019844001159071922, -0.03359175845980644, 0.10000011324882507),
            (-0.01744179055094719, 0.997369647026062, 0.07035345584154129, 0.1999998837709427),
            (0.034899525344371796, -0.06971397250890732, 0.9969563484191895, 0.3000001311302185),
            (0.0, 0.0, 0.0, 1.0),
        )))

        bpy.ops.constraint.childof_clear_inverse(context, constraint='Child Of')
        self.matrix_test('Child Of.object.owner', initial_matrix)

    def test_object_rotation_only(self):
        """Child Of: rotation only."""
        owner = bpy.context.scene.objects['Child Of.object.owner']
        constraint = owner.constraints['Child Of']
        constraint.use_location_x = constraint.use_location_y = constraint.use_location_z = False
        constraint.use_scale_x = constraint.use_scale_y = constraint.use_scale_z = False

        initial_matrix = Matrix((
            (0.8340795636177063, -0.4500490725040436, 0.31900957226753235, 0.10000000149011612),
            (0.4547243118286133, 0.8883093595504761, 0.06428192555904388, 0.20000000298023224),
            (-0.31230923533439636, 0.09144517779350281, 0.9455690383911133, 0.30000001192092896),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Child Of.object.owner', initial_matrix)

        context = self.constraint_context('Child Of', owner_name='Child Of.object.owner')
        bpy.ops.constraint.childof_set_inverse(context, constraint='Child Of')
        self.matrix_test('Child Of.object.owner', Matrix((
            (0.9992386102676392, 0.019843975082039833, -0.033591702580451965, 0.10000000149011612),
            (-0.017441781237721443, 0.9973695874214172, 0.0703534483909607, 0.20000000298023224),
            (0.03489946573972702, -0.06971397250890732, 0.9969563484191895, 0.30000001192092896),
            (0.0, 0.0, 0.0, 1.0),
        )))

        bpy.ops.constraint.childof_clear_inverse(context, constraint='Child Of')
        self.matrix_test('Child Of.object.owner', initial_matrix)

    def test_object_no_x_axis(self):
        """Child Of: loc/rot/scale on only Y and Z axes."""
        owner = bpy.context.scene.objects['Child Of.object.owner']
        constraint = owner.constraints['Child Of']
        constraint.use_location_x = False
        constraint.use_rotation_x = False
        constraint.use_scale_x = False

        initial_matrix = Matrix((
            (0.8294582366943359, -0.4013831615447998, 0.2102886438369751, 0.10000000149011612),
            (0.46277597546577454, 0.6895919442176819, 0.18639995157718658, 2.2317214012145996),
            (-0.31224438548088074, -0.06574578583240509, 0.8546382784843445, 3.219514846801758),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Child Of.object.owner', initial_matrix)

        context = self.constraint_context('Child Of', owner_name='Child Of.object.owner')
        bpy.ops.constraint.childof_set_inverse(context, constraint='Child Of')
        self.matrix_test('Child Of.object.owner', Matrix((
            (0.9992386102676392, 0.019843991845846176, -0.03359176218509674, 0.10000000149011612),
            (-0.017441775649785995, 0.997369647026062, 0.0703534483909607, 0.2000001221895218),
            (0.034899499267339706, -0.06971398741006851, 0.996956467628479, 0.3000001311302185),
            (0.0, 0.0, 0.0, 1.0),
        )))

        bpy.ops.constraint.childof_clear_inverse(context, constraint='Child Of')
        self.matrix_test('Child Of.object.owner', initial_matrix)

    def test_bone_simple_parent(self):
        """Child Of: simple evaluation of bone parent."""
        initial_matrix = Matrix((
            (0.5133683681488037, -0.06418320536613464, -0.014910104684531689, -0.2277737855911255),
            (-0.03355155512690544, 0.12542974948883057, -1.080492377281189, 2.082599639892578),
            (0.019313642755150795, 1.0446348190307617, 0.1244703009724617, 2.8542778491973877),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Child Of.armature.owner', initial_matrix)

        context = self.constraint_context('Child Of', owner_name='Child Of.armature.owner')
        bpy.ops.constraint.childof_set_inverse(context, constraint='Child Of')

        self.matrix_test('Child Of.armature.owner', Matrix((
            (0.9992386102676392, 0.019843988120555878, -0.03359176218509674, 0.8358089923858643),
            (-0.017441775649785995, 0.997369647026062, 0.0703534483909607, 1.7178752422332764),
            (0.03489949554204941, -0.06971397995948792, 0.9969563484191895, -1.8132872581481934),
            (0.0, 0.0, 0.0, 1.0),
        )))

        bpy.ops.constraint.childof_clear_inverse(context, constraint='Child Of')
        self.matrix_test('Child Of.armature.owner', initial_matrix)

    def test_vertexgroup_simple_parent(self):
        """Child Of: simple evaluation of vertex group parent."""
        initial_matrix = Matrix((
            (-0.8076590895652771, 0.397272527217865, 0.4357309341430664, 1.188504934310913),
            (-0.4534659683704376, -0.8908230066299438, -0.028334975242614746, 1.7851561307907104),
            (0.3769024908542633, -0.22047416865825653, 0.8996308445930481, 3.4457669258117676),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Child Of.vertexgroup.owner', initial_matrix)

        context = self.constraint_context('Child Of', owner_name='Child Of.vertexgroup.owner')
        bpy.ops.constraint.childof_set_inverse(context, constraint='Child Of')

        self.matrix_test('Child Of.vertexgroup.owner', Matrix((
            (0.9992386102676392, 0.019843988120555878, -0.03359176218509674, 0.10000000149011612),
            (-0.017441775649785995, 0.997369647026062, 0.0703534483909607, 0.20000000298023224),
            (0.03489949554204941, -0.06971397995948792, 0.9969563484191895, 0.30000001192092896),
            (0.0, 0.0, 0.0, 1.0),
        )))

        bpy.ops.constraint.childof_clear_inverse(context, constraint='Child Of')
        self.matrix_test('Child Of.vertexgroup.owner', initial_matrix)


class ObjectSolverTest(AbstractConstraintTests):
    layer_collection = 'Object Solver'

    def test_simple_parent(self):
        """Object Solver: simple evaluation of parent."""
        initial_matrix = Matrix((
            (-0.970368504524231, 0.03756079450249672, -0.23869234323501587, -0.681557297706604),
            (-0.24130365252494812, -0.2019258439540863, 0.9492092132568359, 6.148940086364746),
            (-0.012545102275907993, 0.9786801934242249, 0.20500603318214417, 2.2278831005096436),
            (0.0, 0.0, 0.0, 1.0),
        ))
        self.matrix_test('Object Solver.owner', initial_matrix)

        context = self.constraint_context('Object Solver')
        bpy.ops.constraint.objectsolver_set_inverse(context, constraint='Object Solver')
        self.matrix_test('Object Solver.owner', Matrix((
            (0.9992387294769287, 0.019843989983201027, -0.03359176591038704, 0.10000025480985641),
            (-0.017441747710108757, 0.9973697662353516, 0.07035345584154129, 0.1999993920326233),
            (0.034899502992630005, -0.06971398741006851, 0.996956467628479, 0.29999980330467224),
            (0.0, 0.0, 0.0, 1.0),
        )))

        bpy.ops.constraint.objectsolver_clear_inverse(context, constraint='Object Solver')
        self.matrix_test('Object Solver.owner', initial_matrix)


def main():
    global args
    import argparse

    if '--' in sys.argv:
        argv = [sys.argv[0]] + sys.argv[sys.argv.index('--') + 1:]
    else:
        argv = sys.argv

    parser = argparse.ArgumentParser()
    parser.add_argument('--testdir', required=True, type=pathlib.Path)
    args, remaining = parser.parse_known_args(argv)

    unittest.main(argv=remaining)


if __name__ == "__main__":
    main()
