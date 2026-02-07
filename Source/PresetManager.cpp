#include "PresetManager.h"
#include "MappingDefinition.h"

PresetManager::PresetManager() {
  ensureStaticLayers();
  migrateToLayerHierarchy();
}

PresetManager::~PresetManager() {}

void PresetManager::beginTransaction() {
  isLoading = true;
}

void PresetManager::endTransaction() {
  isLoading = false;
  sendChangeMessage();
}

void PresetManager::migrateToLayerHierarchy() {
  auto oldMappings = rootNode.getChildWithName("Mappings");
  if (oldMappings.isValid() && oldMappings.getNumChildren() > 0) {
    // Old structure exists, migrate to Layer 0
    auto layer0 = getLayerNode(0);
    auto layer0Mappings = layer0.getChildWithName("Mappings");

    if (!layer0Mappings.isValid()) {
      layer0Mappings = juce::ValueTree("Mappings");
      layer0.addChild(layer0Mappings, -1, nullptr);
    }

    // Copy all mappings to Layer 0
    for (int i = 0; i < oldMappings.getNumChildren(); ++i) {
      auto mapping = oldMappings.getChild(i);
      if (mapping.isValid()) {
        // Set layerID to 0 if not already set
        if (!mapping.hasProperty("layerID")) {
          mapping.setProperty("layerID", 0, nullptr);
        }
        layer0Mappings.addChild(mapping.createCopy(), -1, nullptr);
      }
    }

    // Remove old flat structure
    rootNode.removeChild(oldMappings, nullptr);
  }
}

void PresetManager::saveToFile(juce::File file) {
  saveToFile(file, juce::ValueTree());
}

void PresetManager::saveToFile(juce::File file,
                               const juce::ValueTree &touchpadMixersTree) {
  juce::ValueTree copy = rootNode.createCopy();
  if (touchpadMixersTree.isValid()) {
    copy.addChild(touchpadMixersTree.createCopy(), -1, nullptr);
  }
  if (auto xml = copy.createXml()) {
    xml->writeTo(file);
  }
}

juce::ValueTree PresetManager::getTouchpadMixersNode() const {
  return rootNode.getChildWithName("TouchpadMixers");
}

namespace {
// Phase 56.1: Migrate CC/Envelope to Expression
void migrateMappingTypes(juce::ValueTree tree) {
  if (tree.hasType("Mapping")) {
    juce::var typeVar = tree.getProperty("type");
    juce::String typeStr =
        typeVar.isString() ? typeVar.toString().trim() : juce::String();
    int typeInt = typeVar.isInt() ? static_cast<int>(typeVar) : -1;

    if (typeStr.equalsIgnoreCase("CC") || typeInt == 1) {
      tree.setProperty("type", "Expression", nullptr);
      tree.setProperty("useCustomEnvelope", false, nullptr);
      if (!tree.hasProperty("adsrTarget"))
        tree.setProperty("adsrTarget", "CC", nullptr);
    } else if (typeStr.equalsIgnoreCase("Envelope") || typeInt == 4) {
      tree.setProperty("type", "Expression", nullptr);
      tree.setProperty("useCustomEnvelope", true, nullptr);
    }
  }
  for (int i = 0; i < tree.getNumChildren(); ++i)
    migrateMappingTypes(tree.getChild(i));
}
} // namespace

void PresetManager::loadFromFile(juce::File file) {
  isLoading = true; // Phase 41.1: suspend listener reactions during load

  auto xml = juce::XmlDocument::parse(file);
  if (xml != nullptr) {
    auto newTree = juce::ValueTree::fromXml(*xml);
    if (newTree.isValid() && newTree.hasType(rootNode.getType())) {
      migrateMappingTypes(newTree);
      rootNode.copyPropertiesFrom(newTree, nullptr);
      rootNode.removeAllChildren(nullptr);
      for (int i = 0; i < newTree.getNumChildren(); ++i) {
        auto child = newTree.getChild(i).createCopy();
        // Sanitize: remove layers with id < 0 or id > 8 to avoid corrupt data
        if (child.hasType("Layers")) {
          for (int j = child.getNumChildren() - 1; j >= 0; --j) {
            auto layer = child.getChild(j);
            int id = static_cast<int>(layer.getProperty("id", -1));
            if (id < 0 || id > 8) {
              DBG("PresetManager: Removing invalid layer ID " + juce::String(id));
              child.removeChild(j, nullptr);
            }
          }
        }
        rootNode.addChild(child, -1, nullptr);
      }

      // Phase 41.2: Sanitize – keep only first "Layers" child if duplicates exist
      juce::Array<int> layersIndices;
      for (int i = 0; i < rootNode.getNumChildren(); ++i)
        if (rootNode.getChild(i).hasType("Layers"))
          layersIndices.add(i);
      for (int k = layersIndices.size() - 1; k >= 1; --k)
        rootNode.removeChild(layersIndices[k], nullptr);

      ensureStaticLayers();
      migrateToLayerHierarchy();
    }
  }

  isLoading = false;
  sendChangeMessage(); // one broadcast so listeners rebuild once
}

void PresetManager::ensureStaticLayers() {
  auto layersList = rootNode.getChildWithName("Layers");
  if (!layersList.isValid()) {
    layersList = juce::ValueTree("Layers");
    rootNode.addChild(layersList, -1, nullptr);
  }

  // Phase 41.2: Remove any layer with id > 8 (enforce static limit)
  for (int j = layersList.getNumChildren() - 1; j >= 0; --j) {
    auto layer = layersList.getChild(j);
    if (layer.isValid() && static_cast<int>(layer.getProperty("id", -1)) > 8)
      layersList.removeChild(layer, nullptr);
  }

  for (int i = 0; i <= 8; ++i) {
    bool found = false;
    for (int j = 0; j < layersList.getNumChildren(); ++j) {
      auto layer = layersList.getChild(j);
      if (layer.isValid() &&
          static_cast<int>(layer.getProperty("id", -1)) == i) {
        found = true;
        break;
      }
    }
    if (!found) {
      juce::ValueTree layer("Layer");
      layer.setProperty("id", i, nullptr);
      layer.setProperty("name", i == 0 ? "Base" : "Layer " + juce::String(i),
                        nullptr);
      layer.setProperty("soloLayer", false, nullptr);
      layer.setProperty("passthruInheritance", false, nullptr);
      layer.setProperty("privateToLayer", false, nullptr);
      juce::ValueTree mappings("Mappings");
      layer.addChild(mappings, -1, nullptr);
      layersList.addChild(layer, -1, nullptr);
    }
  }
}

juce::ValueTree PresetManager::getLayersList() {
  auto layersList = rootNode.getChildWithName("Layers");
  if (!layersList.isValid()) {
    layersList = juce::ValueTree("Layers");
    rootNode.addChild(layersList, -1, nullptr);
  }
  return layersList;
}

juce::ValueTree PresetManager::getLayerNode(int layerIndex) {
  if (layerIndex < 0 || layerIndex > 8)
    return juce::ValueTree();
  auto layersList = getLayersList();
  for (int i = 0; i < layersList.getNumChildren(); ++i) {
    auto layer = layersList.getChild(i);
    if (layer.isValid() &&
        static_cast<int>(layer.getProperty("id", -1)) == layerIndex)
      return layer;
  }
  return juce::ValueTree();
}

juce::ValueTree PresetManager::getMappingsListForLayer(int layerIndex) {
  auto layer = getLayerNode(layerIndex);
  if (!layer.isValid())
    return juce::ValueTree();
  auto mappings = layer.getChildWithName("Mappings");
  if (!mappings.isValid()) {
    mappings = juce::ValueTree("Mappings");
    layer.addChild(mappings, -1, nullptr);
  }
  return mappings;
}

std::vector<juce::ValueTree> PresetManager::getEnabledMappingsForLayer(
    int layerIndex) const {
  std::vector<juce::ValueTree> out;
  juce::ValueTree layer = const_cast<PresetManager *>(this)->getLayerNode(layerIndex);
  if (!layer.isValid())
    return out;
  juce::ValueTree mappings = layer.getChildWithName("Mappings");
  if (!mappings.isValid())
    return out;
  for (int i = 0; i < mappings.getNumChildren(); ++i) {
    auto child = mappings.getChild(i);
    if (child.isValid() && MappingDefinition::isMappingEnabled(child))
      out.push_back(child);
  }
  return out;
}

juce::ValueTree PresetManager::getMappingsNode() {
  // Legacy: Return Layer 0's mappings for backward compatibility
  return getMappingsListForLayer(0);
}