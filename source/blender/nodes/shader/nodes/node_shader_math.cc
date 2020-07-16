/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.h"

/* **************** SCALAR MATH ******************** */
static bNodeSocketTemplate sh_node_math_in[] = {
    {SOCK_FLOAT, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 0.5f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {SOCK_FLOAT, N_("Value"), 0.0f, 0.5f, 0.5f, 1.0f, -10000.0f, 10000.0f, PROP_NONE},
    {-1, ""}};

static bNodeSocketTemplate sh_node_math_out[] = {{SOCK_FLOAT, N_("Value")}, {-1, ""}};

static const char *gpu_shader_get_name(int mode)
{
  switch (mode) {
    case NODE_MATH_ADD:
      return "math_add";
    case NODE_MATH_SUBTRACT:
      return "math_subtract";
    case NODE_MATH_MULTIPLY:
      return "math_multiply";
    case NODE_MATH_DIVIDE:
      return "math_divide";
    case NODE_MATH_MULTIPLY_ADD:
      return "math_multiply_add";

    case NODE_MATH_POWER:
      return "math_power";
    case NODE_MATH_LOGARITHM:
      return "math_logarithm";
    case NODE_MATH_EXPONENT:
      return "math_exponent";
    case NODE_MATH_SQRT:
      return "math_sqrt";
    case NODE_MATH_INV_SQRT:
      return "math_inversesqrt";
    case NODE_MATH_ABSOLUTE:
      return "math_absolute";
    case NODE_MATH_RADIANS:
      return "math_radians";
    case NODE_MATH_DEGREES:
      return "math_degrees";

    case NODE_MATH_MINIMUM:
      return "math_minimum";
    case NODE_MATH_MAXIMUM:
      return "math_maximum";
    case NODE_MATH_LESS_THAN:
      return "math_less_than";
    case NODE_MATH_GREATER_THAN:
      return "math_greater_than";
    case NODE_MATH_SIGN:
      return "math_sign";
    case NODE_MATH_COMPARE:
      return "math_compare";
    case NODE_MATH_SMOOTH_MIN:
      return "math_smoothmin";
    case NODE_MATH_SMOOTH_MAX:
      return "math_smoothmax";

    case NODE_MATH_ROUND:
      return "math_round";
    case NODE_MATH_FLOOR:
      return "math_floor";
    case NODE_MATH_CEIL:
      return "math_ceil";
    case NODE_MATH_FRACTION:
      return "math_fraction";
    case NODE_MATH_MODULO:
      return "math_modulo";
    case NODE_MATH_TRUNC:
      return "math_trunc";
    case NODE_MATH_SNAP:
      return "math_snap";
    case NODE_MATH_WRAP:
      return "math_wrap";
    case NODE_MATH_PINGPONG:
      return "math_pingpong";

    case NODE_MATH_SINE:
      return "math_sine";
    case NODE_MATH_COSINE:
      return "math_cosine";
    case NODE_MATH_TANGENT:
      return "math_tangent";
    case NODE_MATH_SINH:
      return "math_sinh";
    case NODE_MATH_COSH:
      return "math_cosh";
    case NODE_MATH_TANH:
      return "math_tanh";
    case NODE_MATH_ARCSINE:
      return "math_arcsine";
    case NODE_MATH_ARCCOSINE:
      return "math_arccosine";
    case NODE_MATH_ARCTANGENT:
      return "math_arctangent";
    case NODE_MATH_ARCTAN2:
      return "math_arctan2";
  }
  return nullptr;
}

static int gpu_shader_math(GPUMaterial *mat,
                           bNode *node,
                           bNodeExecData *UNUSED(execdata),
                           GPUNodeStack *in,
                           GPUNodeStack *out)
{
  const char *name = gpu_shader_get_name(node->custom1);
  if (name != nullptr) {
    int ret = GPU_stack_link(mat, node, name, in, out);

    if (ret && node->custom2 & SHD_MATH_CLAMP) {
      float min[3] = {0.0f, 0.0f, 0.0f};
      float max[3] = {1.0f, 1.0f, 1.0f};
      GPU_link(
          mat, "clamp_value", out[0].link, GPU_constant(min), GPU_constant(max), &out[0].link);
    }
    return ret;
  }
  else {
    return 0;
  }
}

static void sh_node_math_expand_in_mf_network(blender::bke::NodeMFNetworkBuilder &builder)
{
  /* TODO: Implement clamp and other operations. */
  const int mode = builder.bnode().custom1;
  switch (mode) {
    case NODE_MATH_ADD: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Add", [](float a, float b) { return a + b; }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_SUBTRACT: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Subtract", [](float a, float b) { return a - b; }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_MULTIPLY: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Multiply", [](float a, float b) { return a * b; }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_DIVIDE: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{"Divide", safe_divide};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_MULTIPLY_ADD: {
      static blender::fn::CustomMF_SI_SI_SI_SO<float, float, float, float> fn{
          "Multiply Add", [](float a, float b, float c) { return a * b + c; }};
      builder.set_matching_fn(fn);
      break;
    }

    case NODE_MATH_POWER: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{"Power", safe_powf};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_LOGARITHM: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{"Logarithm", safe_logf};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_EXPONENT: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Exponent", expf};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_SQRT: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Sqrt", safe_sqrtf};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_INV_SQRT: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Inverse Sqrt", safe_inverse_sqrtf};
      builder.set_matching_fn(fn);
      break;
    };
    case NODE_MATH_ABSOLUTE: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Absolute",
                                                          [](float a) { return fabs(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_RADIANS: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Radians",
                                                          [](float a) { return DEG2RAD(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_DEGREES: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Degrees",
                                                          [](float a) { return RAD2DEG(a); }};
      builder.set_matching_fn(fn);
      break;
    }

    case NODE_MATH_MINIMUM: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Minimum", [](float a, float b) { return std::min(a, b); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_MAXIMUM: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Maximum", [](float a, float b) { return std::max(a, b); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_LESS_THAN: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Less Than", [](float a, float b) { return (float)(a < b); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_GREATER_THAN: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Greater Than", [](float a, float b) { return (float)(a > b); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_SIGN: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{
          "Sign", [](float a) { return compatible_signf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_COMPARE: {
      static blender::fn::CustomMF_SI_SI_SI_SO<float, float, float, float> fn{
          "Compare", [](float a, float b, float c) -> float {
            return ((a == b) || (fabsf(a - b) <= fmaxf(c, FLT_EPSILON))) ? 1.0f : 0.0f;
          }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_SMOOTH_MIN: {
      builder.not_implemented();
      break;
    }
    case NODE_MATH_SMOOTH_MAX: {
      builder.not_implemented();
      break;
    }

    case NODE_MATH_ROUND: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{
          "Round", [](float a) { return floorf(a + 0.5f); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_FLOOR: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Floor",
                                                          [](float a) { return floorf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_CEIL: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Ceil",
                                                          [](float a) { return ceilf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_FRACTION: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Fraction",
                                                          [](float a) { return a - floorf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_MODULO: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Modulo", [](float a, float b) { return safe_modf(a, b); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_TRUNC: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{
          "Trunc", [](float a) { return a >= 0.0f ? floorf(a) : ceilf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_SNAP: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Snap", [](float a, float b) { return floorf(safe_divide(a, b)) * b; }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_WRAP: {
      builder.not_implemented();
      break;
    }
    case NODE_MATH_PINGPONG: {
      builder.not_implemented();
      break;
    }

    case NODE_MATH_SINE: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Sine", [](float a) { return sinf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_COSINE: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Cosine",
                                                          [](float a) { return cosf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_TANGENT: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Tangent",
                                                          [](float a) { return tanf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_SINH: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Hyperbolic Sine",
                                                          [](float a) { return sinhf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_COSH: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Hyperbolic Cosine",
                                                          [](float a) { return coshf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_TANH: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Hyperbolic Tangent",
                                                          [](float a) { return tanhf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_ARCSINE: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Arc Sine", safe_asinf};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_ARCCOSINE: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Arc Cosine", safe_acosf};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_ARCTANGENT: {
      static blender::fn::CustomMF_SI_SO<float, float> fn{"Arc Tangent",
                                                          [](float a) { return atanf(a); }};
      builder.set_matching_fn(fn);
      break;
    }
    case NODE_MATH_ARCTAN2: {
      static blender::fn::CustomMF_SI_SI_SO<float, float, float> fn{
          "Arc Tangent 2", [](float a, float b) { return atan2f(a, b); }};
      builder.set_matching_fn(fn);
      break;
    }

    default:
      BLI_assert(false);
      break;
  }
}

void register_node_type_sh_math(void)
{
  static bNodeType ntype;

  sh_fn_node_type_base(&ntype, SH_NODE_MATH, "Math", NODE_CLASS_CONVERTOR, 0);
  node_type_socket_templates(&ntype, sh_node_math_in, sh_node_math_out);
  node_type_label(&ntype, node_math_label);
  node_type_gpu(&ntype, gpu_shader_math);
  node_type_update(&ntype, node_math_update);
  ntype.expand_in_mf_network = sh_node_math_expand_in_mf_network;

  nodeRegisterType(&ntype);
}
