#pragma once
#define DT_NODELABEL(name) name
#define DT_PATH(name) name
#define DT_CAT_INNER(a, b) a##b
#define DT_CAT(a, b) DT_CAT_INNER(a, b)
#define DT_NODE_EXISTS(node) DT_CAT(DT_EXISTS_, node)
#define DT_EXISTS_photo_transistor TEST_PHOTOTRANSISTOR
#define DT_EXISTS_ldr (!TEST_PHOTOTRANSISTOR)
#define DT_PROP(node, property) DT_CAT(DT_PROPERTY_, property)
#define DT_PROPERTY_pulse 5
#define DT_PROPERTY_dry {100000, 0, 0}
#define DT_PROPERTY_wet {1100000, 0, 0}
#define DT_PROPERTY_output_ohms 10000
