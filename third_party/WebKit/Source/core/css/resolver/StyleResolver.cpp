/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/css/resolver/StyleResolver.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "XMLNames.h"
#include "core/animation/AnimatableValue.h"
#include "core/animation/Animation.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSSelectorList.h"
#include "core/css/CSSShaderValue.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/Counter.h"
#include "core/css/DeprecatedStyleBuilder.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/FontFeatureValue.h"
#include "core/css/FontSize.h"
#include "core/css/FontValue.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/Pair.h"
#include "core/css/RuleSet.h"
#include "core/css/ShadowValue.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StylePropertyShorthand.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/resolver/FilterOperationResolver.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/TransformBuilder.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/FullscreenController.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/NodeRenderingContext.h"
#include "core/dom/Text.h"
#include "core/dom/WebCoreMemoryInstrumentation.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/track/WebVTTElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/cache/CachedDocument.h"
#include "core/loader/cache/CachedSVGDocumentReference.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/LinkHash.h"
#include "core/platform/graphics/filters/custom/CustomFilterConstants.h"
#include "core/platform/graphics/filters/custom/CustomFilterOperation.h"
#include "core/platform/text/LocaleToScriptMapping.h"
#include "core/rendering/RenderTheme.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/ContentData.h"
#include "core/rendering/style/CounterContent.h"
#include "core/rendering/style/CursorList.h"
#include "core/rendering/style/KeyframeList.h"
#include "core/rendering/style/QuotesData.h"
#include "core/rendering/style/RenderStyleConstants.h"
#include "core/rendering/style/ShadowData.h"
#include "core/rendering/style/StyleCachedImage.h"
#include "core/rendering/style/StyleCachedImageSet.h"
#include "core/rendering/style/StyleCachedShader.h"
#include "core/rendering/style/StyleCustomFilterProgram.h"
#include "core/rendering/style/StyleCustomFilterProgramCache.h"
#include "core/rendering/style/StyleGeneratedImage.h"
#include "core/rendering/style/StylePendingImage.h"
#include "core/rendering/style/StylePendingShader.h"
#include "core/rendering/style/StyleShader.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGFontFaceElement.h"
#include "wtf/MemoryInstrumentationHashMap.h"
#include "wtf/MemoryInstrumentationVector.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

using namespace std;

namespace WTF {

template<> struct SequenceMemoryInstrumentationTraits<const WebCore::RuleData*> {
    template <typename I> static void reportMemoryUsage(I, I, MemoryClassInfo&) { }
};

}

namespace WebCore {

using namespace HTMLNames;

#define HANDLE_INHERIT(prop, Prop) \
if (isInherit) { \
    m_state.style()->set##Prop(m_state.parentStyle()->prop()); \
    return; \
}

#define HANDLE_INHERIT_AND_INITIAL(prop, Prop) \
HANDLE_INHERIT(prop, Prop) \
if (isInitial) { \
    m_state.style()->set##Prop(RenderStyle::initial##Prop()); \
    return; \
}

RenderStyle* StyleResolver::s_styleNotYetAvailable;

static StylePropertySet* leftToRightDeclaration()
{
    DEFINE_STATIC_LOCAL(RefPtr<MutableStylePropertySet>, leftToRightDecl, (MutableStylePropertySet::create()));
    if (leftToRightDecl->isEmpty())
        leftToRightDecl->setProperty(CSSPropertyDirection, CSSValueLtr);
    return leftToRightDecl.get();
}

static StylePropertySet* rightToLeftDeclaration()
{
    DEFINE_STATIC_LOCAL(RefPtr<MutableStylePropertySet>, rightToLeftDecl, (MutableStylePropertySet::create()));
    if (rightToLeftDecl->isEmpty())
        rightToLeftDecl->setProperty(CSSPropertyDirection, CSSValueRtl);
    return rightToLeftDecl.get();
}


void StyleResolver::MatchResult::addMatchedProperties(const StylePropertySet* properties, StyleRule* rule, unsigned linkMatchType, PropertyWhitelistType propertyWhitelistType)
{
    matchedProperties.grow(matchedProperties.size() + 1);
    StyleResolver::MatchedProperties& newProperties = matchedProperties.last();
    newProperties.properties = const_cast<StylePropertySet*>(properties);
    newProperties.linkMatchType = linkMatchType;
    newProperties.whitelistType = propertyWhitelistType;
    matchedRules.append(rule);
}

StyleResolver::StyleResolver(Document* document, bool matchAuthorAndUserStyles)
    : m_matchedPropertiesCacheAdditionsSinceLastSweep(0)
    , m_matchedPropertiesCacheSweepTimer(this, &StyleResolver::sweepMatchedPropertiesCache)
    , m_document(document)
    , m_matchAuthorAndUserStyles(matchAuthorAndUserStyles)
    , m_fontSelector(CSSFontSelector::create(document))
    , m_viewportStyleResolver(ViewportStyleResolver::create(document))
    , m_styleBuilder(DeprecatedStyleBuilder::sharedStyleBuilder())
    , m_styleMap(this)
{
    Element* root = document->documentElement();

    CSSDefaultStyleSheets::initDefaultStyle(root);

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    FrameView* view = document->view();
    if (view)
        m_medium = adoptPtr(new MediaQueryEvaluator(view->mediaType()));
    else
        m_medium = adoptPtr(new MediaQueryEvaluator("all"));

    if (root)
        m_rootDefaultStyle = styleForElement(root, 0, DisallowStyleSharing, MatchOnlyUserAgentRules);

    if (m_rootDefaultStyle && view)
        m_medium = adoptPtr(new MediaQueryEvaluator(view->mediaType(), view->frame(), m_rootDefaultStyle.get()));

    m_styleTree.clear();

    DocumentStyleSheetCollection* styleSheetCollection = document->styleSheetCollection();
    m_ruleSets.initUserStyle(styleSheetCollection, *m_medium, *this);

#if ENABLE(SVG_FONTS)
    if (document->svgExtensions()) {
        const HashSet<SVGFontFaceElement*>& svgFontFaceElements = document->svgExtensions()->svgFontFaceElements();
        HashSet<SVGFontFaceElement*>::const_iterator end = svgFontFaceElements.end();
        for (HashSet<SVGFontFaceElement*>::const_iterator it = svgFontFaceElements.begin(); it != end; ++it)
            fontSelector()->addFontFaceRule((*it)->fontFaceRule());
    }
#endif

    appendAuthorStyleSheets(0, styleSheetCollection->activeAuthorStyleSheets());
}

void StyleResolver::appendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet> >& styleSheets)
{
    // This handles sheets added to the end of the stylesheet list only. In other cases the style resolver
    // needs to be reconstructed. To handle insertions too the rule order numbers would need to be updated.
    unsigned size = styleSheets.size();
    for (unsigned i = firstNew; i < size; ++i) {
        CSSStyleSheet* cssSheet = styleSheets[i].get();
        ASSERT(!cssSheet->disabled());
        if (cssSheet->mediaQueries() && !m_medium->eval(cssSheet->mediaQueries(), this))
            continue;

        StyleSheetContents* sheet = cssSheet->contents();
        ScopedStyleResolver* resolver = ensureScopedStyleResolver(ScopedStyleResolver::scopingNodeFor(cssSheet));
        ASSERT(resolver);
        resolver->addRulesFromSheet(sheet, *m_medium, this);
        m_inspectorCSSOMWrappers.collectFromStyleSheetIfNeeded(cssSheet);
    }

    collectFeatures();

    if (document()->renderer() && document()->renderer()->style())
        document()->renderer()->style()->font().update(fontSelector());

    if (RuntimeEnabledFeatures::cssViewportEnabled())
        viewportStyleResolver()->resolve();
}

void StyleResolver::resetAuthorStyle()
{
    m_styleTree.clear();
}

static PassOwnPtr<RuleSet> makeRuleSet(const Vector<RuleFeature>& rules)
{
    size_t size = rules.size();
    if (!size)
        return nullptr;
    OwnPtr<RuleSet> ruleSet = RuleSet::create();
    for (size_t i = 0; i < size; ++i)
        ruleSet->addRule(rules[i].rule, rules[i].selectorIndex, rules[i].hasDocumentSecurityOrigin ? RuleHasDocumentSecurityOrigin : RuleHasNoSpecialState);
    return ruleSet.release();
}

void StyleResolver::collectFeatures()
{
    m_features.clear();
    m_ruleSets.collectFeaturesTo(m_features, document()->isViewSource());
    m_styleTree.collectFeaturesTo(m_features);

    m_siblingRuleSet = makeRuleSet(m_features.siblingRules);
    m_uncommonAttributeRuleSet = makeRuleSet(m_features.uncommonAttributeRules);
}

void StyleResolver::pushParentElement(Element* parent)
{
    const ContainerNode* parentsParent = parent->parentOrShadowHostElement();

    // We are not always invoked consistently. For example, script execution can cause us to enter
    // style recalc in the middle of tree building. We may also be invoked from somewhere within the tree.
    // Reset the stack in this case, or if we see a new root element.
    // Otherwise just push the new parent.
    if (!parentsParent || m_selectorFilter.parentStackIsEmpty())
        m_selectorFilter.setupParentStack(parent);
    else
        m_selectorFilter.pushParent(parent);

    // Note: We mustn't skip ShadowRoot nodes for the scope stack.
    m_styleTree.pushStyleCache(parent, parent->parentOrShadowHostNode());
}

void StyleResolver::popParentElement(Element* parent)
{
    // Note that we may get invoked for some random elements in some wacky cases during style resolve.
    // Pause maintaining the stack in this case.
    if (m_selectorFilter.parentStackIsConsistent(parent))
        m_selectorFilter.popParent();

    m_styleTree.popStyleCache(parent);
}

void StyleResolver::pushParentShadowRoot(const ShadowRoot* shadowRoot)
{
    ASSERT(shadowRoot->host());
    m_styleTree.pushStyleCache(shadowRoot, shadowRoot->host());
}

void StyleResolver::popParentShadowRoot(const ShadowRoot* shadowRoot)
{
    ASSERT(shadowRoot->host());
    m_styleTree.popStyleCache(shadowRoot);
}

// This is a simplified style setting function for keyframe styles
void StyleResolver::addKeyframeStyle(PassRefPtr<StyleRuleKeyframes> rule)
{
    AtomicString s(rule->name());
    m_keyframesRuleMap.set(s.impl(), rule);
}

StyleResolver::~StyleResolver()
{
    m_fontSelector->clearDocument();
    m_viewportStyleResolver->clearDocument();
}

void StyleResolver::sweepMatchedPropertiesCache(Timer<StyleResolver>*)
{
    // Look for cache entries containing a style declaration with a single ref and remove them.
    // This may happen when an element attribute mutation causes it to generate a new inlineStyle()
    // or presentationAttributeStyle(), potentially leaving this cache with the last ref on the old one.
    Vector<unsigned, 16> toRemove;
    MatchedPropertiesCache::iterator it = m_matchedPropertiesCache.begin();
    MatchedPropertiesCache::iterator end = m_matchedPropertiesCache.end();
    for (; it != end; ++it) {
        Vector<MatchedProperties>& matchedProperties = it->value.matchedProperties;
        for (size_t i = 0; i < matchedProperties.size(); ++i) {
            if (matchedProperties[i].properties->hasOneRef()) {
                toRemove.append(it->key);
                break;
            }
        }
    }
    for (size_t i = 0; i < toRemove.size(); ++i)
        m_matchedPropertiesCache.remove(toRemove[i]);

    m_matchedPropertiesCacheAdditionsSinceLastSweep = 0;
}

bool StyleResolver::classNamesAffectedByRules(const SpaceSplitString& classNames) const
{
    for (unsigned i = 0; i < classNames.size(); ++i) {
        if (m_features.classesInRules.contains(classNames[i].impl()))
            return true;
    }
    return false;
}

inline void StyleResolver::matchShadowDistributedRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (m_ruleSets.shadowDistributedRules().isEmpty())
        return;

    bool previousCanUseFastReject = collector.canUseFastReject();
    SelectorChecker::BehaviorAtBoundary previousBoundary = collector.behaviorAtBoundary();
    collector.setBehaviorAtBoundary(static_cast<SelectorChecker::BehaviorAtBoundary>(SelectorChecker::CrossesBoundary | SelectorChecker::ScopeContainsLastMatchedElement));
    collector.setCanUseFastReject(false);

    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;
    RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();

    Vector<MatchRequest> matchRequests;
    m_ruleSets.shadowDistributedRules().collectMatchRequests(includeEmptyRules, matchRequests);
    for (size_t i = 0; i < matchRequests.size(); ++i)
        collector.collectMatchingRules(matchRequests[i], ruleRange);
    collector.sortAndTransferMatchedRules();

    collector.setBehaviorAtBoundary(previousBoundary);
    collector.setCanUseFastReject(previousCanUseFastReject);
}

void StyleResolver::matchHostRules(ScopedStyleResolver* resolver, ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (m_state.element() != resolver->scopingNode())
        return;
    resolver->matchHostRules(collector, includeEmptyRules);
}

void StyleResolver::matchScopedAuthorRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    // fast path
    if (m_styleTree.hasOnlyScopedResolverForDocument()) {
        m_styleTree.scopedStyleResolverForDocument()->matchAuthorRules(collector, includeEmptyRules, true);
        return;
    }

    Vector<ScopedStyleResolver*, 8> stack;
    m_styleTree.resolveScopedStyles(m_state.element(), stack);
    if (stack.isEmpty())
        return;

    bool applyAuthorStyles = m_state.element()->treeScope()->applyAuthorStyles();
    for (int i = stack.size() - 1; i >= 0; --i)
        stack.at(i)->matchAuthorRules(collector, includeEmptyRules, applyAuthorStyles);

    matchHostRules(stack.first(), collector, includeEmptyRules);
}

void StyleResolver::matchAuthorRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    matchScopedAuthorRules(collector, includeEmptyRules);
    matchShadowDistributedRules(collector, includeEmptyRules);
}

void StyleResolver::matchUserRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (!m_ruleSets.userStyle())
        return;

    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastUserRule = collector.matchedResult().matchedProperties.size() - 1;

    MatchRequest matchRequest(m_ruleSets.userStyle(), includeEmptyRules);
    RuleRange ruleRange = collector.matchedResult().ranges.userRuleRange();
    collector.collectMatchingRules(matchRequest, ruleRange);
    collector.collectMatchingRulesForRegion(matchRequest, ruleRange);

    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchUARules(ElementRuleCollector& collector)
{
    collector.setMatchingUARules(true);

    // First we match rules from the user agent sheet.
    if (CSSDefaultStyleSheets::simpleDefaultStyleSheet)
        collector.matchedResult().isCacheable = false;

    RuleSet* userAgentStyleSheet = m_medium->mediaTypeMatchSpecific("print")
        ? CSSDefaultStyleSheets::defaultPrintStyle : CSSDefaultStyleSheets::defaultStyle;
    matchUARules(collector, userAgentStyleSheet);

    // In quirks mode, we match rules from the quirks user agent sheet.
    if (document()->inQuirksMode())
        matchUARules(collector, CSSDefaultStyleSheets::defaultQuirksStyle);

    // If document uses view source styles (in view source mode or in xml viewer mode), then we match rules from the view source style sheet.
    if (document()->isViewSource())
        matchUARules(collector, CSSDefaultStyleSheets::viewSourceStyle());

    collector.setMatchingUARules(false);
}

void StyleResolver::matchUARules(ElementRuleCollector& collector, RuleSet* rules)
{
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastUARule = collector.matchedResult().matchedProperties.size() - 1;

    RuleRange ruleRange = collector.matchedResult().ranges.UARuleRange();
    collector.collectMatchingRules(MatchRequest(rules), ruleRange);

    collector.sortAndTransferMatchedRules();
}

void StyleResolver::matchAllRules(ElementRuleCollector& collector, bool matchAuthorAndUserStyles, bool includeSMILProperties)
{
    matchUARules(collector);

    // Now we check user sheet rules.
    if (matchAuthorAndUserStyles)
        matchUserRules(collector, false);

    // Now check author rules, beginning first with presentational attributes mapped from HTML.
    if (m_state.styledElement()) {
        collector.addElementStyleProperties(m_state.styledElement()->presentationAttributeStyle());

        // Now we check additional mapped declarations.
        // Tables and table cells share an additional mapped rule that must be applied
        // after all attributes, since their mapped style depends on the values of multiple attributes.
        collector.addElementStyleProperties(m_state.styledElement()->additionalPresentationAttributeStyle());

        if (m_state.styledElement()->isHTMLElement()) {
            bool isAuto;
            TextDirection textDirection = toHTMLElement(m_state.styledElement())->directionalityIfhasDirAutoAttribute(isAuto);
            if (isAuto)
                collector.matchedResult().addMatchedProperties(textDirection == LTR ? leftToRightDeclaration() : rightToLeftDeclaration());
        }
    }

    // Check the rules in author sheets next.
    if (matchAuthorAndUserStyles)
        matchAuthorRules(collector, false);

    // Now check our inline style attribute.
    if (matchAuthorAndUserStyles && m_state.styledElement() && m_state.styledElement()->inlineStyle()) {
        // Inline style is immutable as long as there is no CSSOM wrapper.
        // FIXME: Media control shadow trees seem to have problems with caching.
        bool isInlineStyleCacheable = !m_state.styledElement()->inlineStyle()->isMutable() && !m_state.styledElement()->isInShadowTree();
        // FIXME: Constify.
        collector.addElementStyleProperties(m_state.styledElement()->inlineStyle(), isInlineStyleCacheable);
    }

    // Now check SMIL animation override style.
    if (includeSMILProperties && matchAuthorAndUserStyles && m_state.styledElement() && m_state.styledElement()->isSVGElement())
        collector.addElementStyleProperties(toSVGElement(m_state.styledElement())->animatedSMILStyleProperties(), false /* isCacheable */);

    if (m_state.styledElement() && m_state.styledElement()->hasActiveAnimations())
        collector.matchedResult().isCacheable = false;
}

inline void StyleResolver::initElement(Element* e)
{
    if (m_state.element() != e) {
        m_state.initElement(e);
        if (e && e == e->document()->documentElement()) {
            e->document()->setDirectionSetOnDocumentElement(false);
            e->document()->setWritingModeSetOnDocumentElement(false);
        }
    }
}

static const unsigned cStyleSearchThreshold = 10;
static const unsigned cStyleSearchLevelThreshold = 10;

static inline bool parentElementPreventsSharing(const Element* parentElement)
{
    if (!parentElement)
        return false;
    return parentElement->hasFlagsSetDuringStylingOfChildren();
}

Node* StyleResolver::locateCousinList(Element* parent, unsigned& visitedNodeCount) const
{
    if (visitedNodeCount >= cStyleSearchThreshold * cStyleSearchLevelThreshold)
        return 0;
    if (!parent || !parent->isStyledElement())
        return 0;
    if (parent->hasScopedHTMLStyleChild())
        return 0;
    StyledElement* p = static_cast<StyledElement*>(parent);
    if (p->inlineStyle())
        return 0;
    if (p->isSVGElement() && toSVGElement(p)->animatedSMILStyleProperties())
        return 0;
    if (p->hasID() && m_features.idsInRules.contains(p->idForStyleResolution().impl()))
        return 0;

    RenderStyle* parentStyle = p->renderStyle();
    unsigned subcount = 0;
    Node* thisCousin = p;
    Node* currentNode = p->previousSibling();

    // Reserve the tries for this level. This effectively makes sure that the algorithm
    // will never go deeper than cStyleSearchLevelThreshold levels into recursion.
    visitedNodeCount += cStyleSearchThreshold;
    while (thisCousin) {
        while (currentNode) {
            ++subcount;
            if (!currentNode->hasScopedHTMLStyleChild() && currentNode->renderStyle() == parentStyle && currentNode->lastChild()
                && currentNode->isElementNode() && !parentElementPreventsSharing(toElement(currentNode))
                && !toElement(currentNode)->shadow()
                ) {
                // Adjust for unused reserved tries.
                visitedNodeCount -= cStyleSearchThreshold - subcount;
                return currentNode->lastChild();
            }
            if (subcount >= cStyleSearchThreshold)
                return 0;
            currentNode = currentNode->previousSibling();
        }
        currentNode = locateCousinList(thisCousin->parentElement(), visitedNodeCount);
        thisCousin = currentNode;
    }

    return 0;
}

bool StyleResolver::styleSharingCandidateMatchesRuleSet(RuleSet* ruleSet)
{
    if (!ruleSet)
        return false;

    ElementRuleCollector collector(this, m_state);
    return collector.hasAnyMatchingRules(ruleSet);
}

bool StyleResolver::canShareStyleWithControl(StyledElement* element) const
{
    const StyleResolverState& state = m_state;

    if (!element->hasTagName(inputTag) || !state.element()->hasTagName(inputTag))
        return false;

    HTMLInputElement* thisInputElement = toHTMLInputElement(element);
    HTMLInputElement* otherInputElement = toHTMLInputElement(state.element());
    if (thisInputElement->elementData() != otherInputElement->elementData()) {
        if (thisInputElement->fastGetAttribute(typeAttr) != otherInputElement->fastGetAttribute(typeAttr))
            return false;
        if (thisInputElement->fastGetAttribute(readonlyAttr) != otherInputElement->fastGetAttribute(readonlyAttr))
            return false;
    }

    if (thisInputElement->isAutofilled() != otherInputElement->isAutofilled())
        return false;
    if (thisInputElement->shouldAppearChecked() != otherInputElement->shouldAppearChecked())
        return false;
    if (thisInputElement->shouldAppearIndeterminate() != otherInputElement->shouldAppearIndeterminate())
        return false;
    if (thisInputElement->isRequired() != otherInputElement->isRequired())
        return false;

    if (element->isDisabledFormControl() != state.element()->isDisabledFormControl())
        return false;

    if (element->isDefaultButtonForForm() != state.element()->isDefaultButtonForForm())
        return false;

    if (state.document()->containsValidityStyleRules()) {
        bool willValidate = element->willValidate();

        if (willValidate != state.element()->willValidate())
            return false;

        if (willValidate && (element->isValidFormControlElement() != state.element()->isValidFormControlElement()))
            return false;

        if (element->isInRange() != state.element()->isInRange())
            return false;

        if (element->isOutOfRange() != state.element()->isOutOfRange())
            return false;
    }

    return true;
}

static inline bool elementHasDirectionAuto(Element* element)
{
    // FIXME: This line is surprisingly hot, we may wish to inline hasDirectionAuto into StyleResolver.
    return element->isHTMLElement() && toHTMLElement(element)->hasDirectionAuto();
}

bool StyleResolver::sharingCandidateHasIdenticalStyleAffectingAttributes(StyledElement* sharingCandidate) const
{
    const StyleResolverState& state = m_state;
    if (state.element()->elementData() == sharingCandidate->elementData())
        return true;
    if (state.element()->fastGetAttribute(XMLNames::langAttr) != sharingCandidate->fastGetAttribute(XMLNames::langAttr))
        return false;
    if (state.element()->fastGetAttribute(langAttr) != sharingCandidate->fastGetAttribute(langAttr))
        return false;

    if (!state.elementAffectedByClassRules()) {
        if (sharingCandidate->hasClass() && classNamesAffectedByRules(sharingCandidate->classNames()))
            return false;
    } else if (sharingCandidate->hasClass()) {
        // SVG elements require a (slow!) getAttribute comparision because "class" is an animatable attribute for SVG.
        if (state.element()->isSVGElement()) {
            if (state.element()->getAttribute(classAttr) != sharingCandidate->getAttribute(classAttr))
                return false;
        } else if (state.element()->classNames() != sharingCandidate->classNames()) {
            return false;
        }
    } else {
        return false;
    }

    if (state.styledElement()->presentationAttributeStyle() != sharingCandidate->presentationAttributeStyle())
        return false;

    if (state.element()->hasTagName(progressTag)) {
        if (state.element()->shouldAppearIndeterminate() != sharingCandidate->shouldAppearIndeterminate())
            return false;
    }

    return true;
}

bool StyleResolver::canShareStyleWithElement(StyledElement* element) const
{
    RenderStyle* style = element->renderStyle();
    const StyleResolverState& state = m_state;

    if (!style)
        return false;
    if (style->unique())
        return false;
    if (style->hasUniquePseudoStyle())
        return false;
    if (element->tagQName() != state.element()->tagQName())
        return false;
    if (element->inlineStyle())
        return false;
    if (element->needsStyleRecalc())
        return false;
    if (element->isSVGElement() && toSVGElement(element)->animatedSMILStyleProperties())
        return false;
    if (element->isLink() != state.element()->isLink())
        return false;
    if (element->hovered() != state.element()->hovered())
        return false;
    if (element->active() != state.element()->active())
        return false;
    if (element->focused() != state.element()->focused())
        return false;
    if (element->shadowPseudoId() != state.element()->shadowPseudoId())
        return false;
    if (element == element->document()->cssTarget())
        return false;
    if (!sharingCandidateHasIdenticalStyleAffectingAttributes(element))
        return false;
    if (element->additionalPresentationAttributeStyle() != state.styledElement()->additionalPresentationAttributeStyle())
        return false;

    if (element->hasID() && m_features.idsInRules.contains(element->idForStyleResolution().impl()))
        return false;
    if (element->hasScopedHTMLStyleChild())
        return false;

    // FIXME: We should share style for option and optgroup whenever possible.
    // Before doing so, we need to resolve issues in HTMLSelectElement::recalcListItems
    // and RenderMenuList::setText. See also https://bugs.webkit.org/show_bug.cgi?id=88405
    if (element->hasTagName(optionTag) || element->hasTagName(optgroupTag))
        return false;

    bool isControl = element->isFormControlElement();

    if (isControl != state.element()->isFormControlElement())
        return false;

    if (isControl && !canShareStyleWithControl(element))
        return false;

    if (style->transitions() || style->animations())
        return false;

    // Turn off style sharing for elements that can gain layers for reasons outside of the style system.
    // See comments in RenderObject::setStyle().
    if (element->hasTagName(iframeTag) || element->hasTagName(frameTag) || element->hasTagName(embedTag) || element->hasTagName(objectTag) || element->hasTagName(appletTag) || element->hasTagName(canvasTag))
        return false;

    if (elementHasDirectionAuto(element))
        return false;

    if (element->isLink() && state.elementLinkState() != style->insideLink())
        return false;

    if (element->isUnresolvedCustomElement() != state.element()->isUnresolvedCustomElement())
        return false;

    // Deny sharing styles between WebVTT and non-WebVTT nodes.
    if (element->isWebVTTElement() != state.element()->isWebVTTElement())
        return false;

    if (element->isWebVTTElement() && state.element()->isWebVTTElement() && toWebVTTElement(element)->isPastNode() != toWebVTTElement(state.element())->isPastNode())
        return false;

    if (FullscreenController* fullscreen = FullscreenController::fromIfExists(state.document())) {
        if (element == fullscreen->webkitCurrentFullScreenElement() || state.element() == fullscreen->webkitCurrentFullScreenElement())
            return false;
    }

    return true;
}

inline StyledElement* StyleResolver::findSiblingForStyleSharing(Node* node, unsigned& count) const
{
    for (; node; node = node->previousSibling()) {
        if (!node->isStyledElement())
            continue;
        if (canShareStyleWithElement(static_cast<StyledElement*>(node)))
            break;
        if (count++ == cStyleSearchThreshold)
            return 0;
    }
    return static_cast<StyledElement*>(node);
}

RenderStyle* StyleResolver::locateSharedStyle()
{
    StyleResolverState& state = m_state;
    if (!state.styledElement() || !state.parentStyle())
        return 0;

    // If the element has inline style it is probably unique.
    if (state.styledElement()->inlineStyle())
        return 0;
    if (state.styledElement()->isSVGElement() && toSVGElement(state.styledElement())->animatedSMILStyleProperties())
        return 0;
    // Ids stop style sharing if they show up in the stylesheets.
    if (state.styledElement()->hasID() && m_features.idsInRules.contains(state.styledElement()->idForStyleResolution().impl()))
        return 0;
    if (parentElementPreventsSharing(state.element()->parentElement()))
        return 0;
    if (state.styledElement()->hasScopedHTMLStyleChild())
        return 0;
    if (state.element() == state.document()->cssTarget())
        return 0;
    if (elementHasDirectionAuto(state.element()))
        return 0;
    if (state.element()->hasActiveAnimations())
        return 0;
    // When a dialog is first shown, its style is mutated to center it in the
    // viewport. So the styles can't be shared since the viewport position and
    // size may be different each time a dialog is opened.
    if (state.element()->hasTagName(dialogTag))
        return 0;

    // Cache whether state.element is affected by any known class selectors.
    // FIXME: This shouldn't be a member variable. The style sharing code could be factored out of StyleResolver.
    state.setElementAffectedByClassRules(state.element() && state.element()->hasClass() && classNamesAffectedByRules(state.element()->classNames()));

    // Check previous siblings and their cousins.
    unsigned count = 0;
    unsigned visitedNodeCount = 0;
    StyledElement* shareElement = 0;
    Node* cousinList = state.styledElement()->previousSibling();
    while (cousinList) {
        shareElement = findSiblingForStyleSharing(cousinList, count);
        if (shareElement)
            break;
        cousinList = locateCousinList(cousinList->parentElement(), visitedNodeCount);
    }

    // If we have exhausted all our budget or our cousins.
    if (!shareElement)
        return 0;

    // Can't share if sibling rules apply. This is checked at the end as it should rarely fail.
    if (styleSharingCandidateMatchesRuleSet(m_siblingRuleSet.get()))
        return 0;
    // Can't share if attribute rules apply.
    if (styleSharingCandidateMatchesRuleSet(m_uncommonAttributeRuleSet.get()))
        return 0;
    // Tracking child index requires unique style for each node. This may get set by the sibling rule match above.
    if (parentElementPreventsSharing(state.element()->parentElement()))
        return 0;
    return shareElement->renderStyle();
}

static void setStylesForPaginationMode(Pagination::Mode paginationMode, RenderStyle* style)
{
    if (paginationMode == Pagination::Unpaginated)
        return;

    switch (paginationMode) {
    case Pagination::LeftToRightPaginated:
        style->setColumnAxis(HorizontalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isLeftToRightDirection() ? NormalColumnProgression : ReverseColumnProgression);
        else
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? ReverseColumnProgression : NormalColumnProgression);
        break;
    case Pagination::RightToLeftPaginated:
        style->setColumnAxis(HorizontalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isLeftToRightDirection() ? ReverseColumnProgression : NormalColumnProgression);
        else
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? NormalColumnProgression : ReverseColumnProgression);
        break;
    case Pagination::TopToBottomPaginated:
        style->setColumnAxis(VerticalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? ReverseColumnProgression : NormalColumnProgression);
        else
            style->setColumnProgression(style->isLeftToRightDirection() ? NormalColumnProgression : ReverseColumnProgression);
        break;
    case Pagination::BottomToTopPaginated:
        style->setColumnAxis(VerticalColumnAxis);
        if (style->isHorizontalWritingMode())
            style->setColumnProgression(style->isFlippedBlocksWritingMode() ? NormalColumnProgression : ReverseColumnProgression);
        else
            style->setColumnProgression(style->isLeftToRightDirection() ? ReverseColumnProgression : NormalColumnProgression);
        break;
    case Pagination::Unpaginated:
        ASSERT_NOT_REACHED();
        break;
    }
}

static void getFontAndGlyphOrientation(const RenderStyle* style, FontOrientation& fontOrientation, NonCJKGlyphOrientation& glyphOrientation)
{
    if (style->isHorizontalWritingMode()) {
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    }

    switch (style->textOrientation()) {
    case TextOrientationVerticalRight:
        fontOrientation = Vertical;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    case TextOrientationUpright:
        fontOrientation = Vertical;
        glyphOrientation = NonCJKGlyphOrientationUpright;
        return;
    case TextOrientationSideways:
        if (style->writingMode() == LeftToRightWritingMode) {
            // FIXME: This should map to sideways-left, which is not supported yet.
            fontOrientation = Vertical;
            glyphOrientation = NonCJKGlyphOrientationVerticalRight;
            return;
        }
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    case TextOrientationSidewaysRight:
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    default:
        ASSERT_NOT_REACHED();
        fontOrientation = Horizontal;
        glyphOrientation = NonCJKGlyphOrientationVerticalRight;
        return;
    }
}

static float getComputedSizeFromSpecifiedSize(Document* document, RenderStyle* style, bool isAbsoluteSize, float specifiedSize, bool useSVGZoomRules)
{
    float zoomFactor = 1.0f;
    if (!useSVGZoomRules) {
        zoomFactor = style->effectiveZoom();
        if (Frame* frame = document->frame())
            zoomFactor *= frame->textZoomFactor();
    }

    return FontSize::getComputedSizeFromSpecifiedSize(document, zoomFactor, isAbsoluteSize, specifiedSize);
}

PassRefPtr<RenderStyle> StyleResolver::styleForDocument(Document* document, CSSFontSelector* fontSelector)
{
    Frame* frame = document->frame();

    // HTML5 states that seamless iframes should replace default CSS values
    // with values inherited from the containing iframe element. However,
    // some values (such as the case of designMode = "on") still need to
    // be set by this "document style".
    RefPtr<RenderStyle> documentStyle = RenderStyle::create();
    bool seamlessWithParent = document->shouldDisplaySeamlesslyWithParent();
    if (seamlessWithParent) {
        RenderStyle* iframeStyle = document->seamlessParentIFrame()->renderStyle();
        if (iframeStyle)
            documentStyle->inheritFrom(iframeStyle);
    }

    // FIXME: It's not clear which values below we want to override in the seamless case!
    documentStyle->setDisplay(BLOCK);
    if (!seamlessWithParent) {
        documentStyle->setRTLOrdering(document->visuallyOrdered() ? VisualOrder : LogicalOrder);
        documentStyle->setZoom(frame && !document->printing() ? frame->pageZoomFactor() : 1);
        documentStyle->setLocale(document->contentLanguage());
    }
    // This overrides any -webkit-user-modify inherited from the parent iframe.
    documentStyle->setUserModify(document->inDesignMode() ? READ_WRITE : READ_ONLY);

    Element* docElement = document->documentElement();
    RenderObject* docElementRenderer = docElement ? docElement->renderer() : 0;
    if (docElementRenderer) {
        // Use the direction and writing-mode of the body to set the
        // viewport's direction and writing-mode unless the property is set on the document element.
        // If there is no body, then use the document element.
        RenderObject* bodyRenderer = document->body() ? document->body()->renderer() : 0;
        if (bodyRenderer && !document->writingModeSetOnDocumentElement())
            documentStyle->setWritingMode(bodyRenderer->style()->writingMode());
        else
            documentStyle->setWritingMode(docElementRenderer->style()->writingMode());
        if (bodyRenderer && !document->directionSetOnDocumentElement())
            documentStyle->setDirection(bodyRenderer->style()->direction());
        else
            documentStyle->setDirection(docElementRenderer->style()->direction());
    }

    if (frame) {
        if (FrameView* frameView = frame->view()) {
            const Pagination& pagination = frameView->pagination();
            if (pagination.mode != Pagination::Unpaginated) {
                setStylesForPaginationMode(pagination.mode, documentStyle.get());
                documentStyle->setColumnGap(pagination.gap);
                if (RenderView* view = document->renderView()) {
                    if (view->hasColumns())
                        view->updateColumnInfoFromStyle(documentStyle.get());
                }
            }
        }
    }

    // Seamless iframes want to inherit their font from their parent iframe, so early return before setting the font.
    if (seamlessWithParent)
        return documentStyle.release();

    FontDescription fontDescription;
    fontDescription.setScript(localeToScriptCodeForFontSelection(documentStyle->locale()));
    if (Settings* settings = document->settings()) {
        fontDescription.setUsePrinterFont(document->printing());
        fontDescription.setRenderingMode(settings->fontRenderingMode());
        const AtomicString& standardFont = settings->standardFontFamily(fontDescription.script());
        if (!standardFont.isEmpty()) {
            fontDescription.setGenericFamily(FontDescription::StandardFamily);
            fontDescription.firstFamily().setFamily(standardFont);
            fontDescription.firstFamily().appendFamily(0);
        }
        fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
        int size = FontSize::fontSizeForKeyword(document, CSSValueMedium, false);
        fontDescription.setSpecifiedSize(size);
        bool useSVGZoomRules = document->isSVGDocument();
        fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(document, documentStyle.get(), fontDescription.isAbsoluteSize(), size, useSVGZoomRules));
    } else
        fontDescription.setUsePrinterFont(document->printing());

    FontOrientation fontOrientation;
    NonCJKGlyphOrientation glyphOrientation;
    getFontAndGlyphOrientation(documentStyle.get(), fontOrientation, glyphOrientation);
    fontDescription.setOrientation(fontOrientation);
    fontDescription.setNonCJKGlyphOrientation(glyphOrientation);

    documentStyle->setFontDescription(fontDescription);
    documentStyle->font().update(fontSelector);

    return documentStyle.release();
}

static inline bool isAtShadowBoundary(const Element* element)
{
    if (!element)
        return false;
    ContainerNode* parentNode = element->parentNode();
    return parentNode && parentNode->isShadowRoot();
}

PassRefPtr<RenderStyle> StyleResolver::styleForElement(Element* element, RenderStyle* defaultParent,
    StyleSharingBehavior sharingBehavior, RuleMatchingBehavior matchingBehavior, RenderRegion* regionForStyling)
{
    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (sharingBehavior == AllowStyleSharing && !element->document()->haveStylesheetsLoaded() && !element->renderer()) {
        if (!s_styleNotYetAvailable) {
            s_styleNotYetAvailable = RenderStyle::create().leakRef();
            s_styleNotYetAvailable->setDisplay(NONE);
            s_styleNotYetAvailable->font().update(m_fontSelector);
        }
        element->document()->setHasNodesWithPlaceholderStyle();
        return s_styleNotYetAvailable;
    }

    StyleResolverState& state = m_state;
    initElement(element);
    state.initForStyleResolve(document(), element, defaultParent, regionForStyling);
    if (sharingBehavior == AllowStyleSharing && !state.distributedToInsertionPoint()) {
        RenderStyle* sharedStyle = locateSharedStyle();
        if (sharedStyle) {
            state.clear();
            return sharedStyle;
        }
    }

    if (state.parentStyle()) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(state.parentStyle(), isAtShadowBoundary(element) ? RenderStyle::AtShadowBoundary : RenderStyle::NotAtShadowBoundary);
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }
    // contenteditable attribute (implemented by -webkit-user-modify) should
    // be propagated from shadow host to distributed node.
    if (state.distributedToInsertionPoint()) {
        if (Element* parent = element->parentElement()) {
            if (RenderStyle* styleOfShadowHost = parent->renderStyle())
                state.style()->setUserModify(styleOfShadowHost->userModify());
        }
    }

    if (element->isLink()) {
        state.style()->setIsLink(true);
        EInsideLink linkState = state.elementLinkState();
        if (linkState != NotInsideLink) {
            bool forceVisited = InspectorInstrumentation::forcePseudoState(element, CSSSelector::PseudoVisited);
            if (forceVisited)
                linkState = InsideVisitedLink;
        }
        state.style()->setInsideLink(linkState);
    }

    bool needsCollection = false;
    CSSDefaultStyleSheets::ensureDefaultStyleSheetsForElement(element, needsCollection);
    if (needsCollection) {
        collectFeatures();
        m_inspectorCSSOMWrappers.reset();
    }

    ElementRuleCollector collector(this, state);
    collector.setRegionForStyling(regionForStyling);

    if (matchingBehavior == MatchOnlyUserAgentRules)
        matchUARules(collector);
    else
        matchAllRules(collector, m_matchAuthorAndUserStyles, matchingBehavior != MatchAllRulesExcludingSMIL);

    applyMatchedProperties(collector.matchedResult(), element);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(state.style(), state.parentStyle(), element);

    state.clear(); // Clear out for the next resolve.

    document()->didAccessStyleResolver();

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForKeyframe(const RenderStyle* elementStyle, const StyleKeyframe* keyframe, KeyframeValue& keyframeValue)
{
    MatchResult result;
    if (keyframe->properties())
        result.addMatchedProperties(keyframe->properties());

    ASSERT(!m_state.style());

    StyleResolverState& state = m_state;

    // Create the style
    state.setStyle(RenderStyle::clone(elementStyle));
    state.setLineHeightValue(0);

    // We don't need to bother with !important. Since there is only ever one
    // decl, there's nothing to override. So just add the first properties.
    bool inheritedOnly = false;
    if (keyframe->properties())
        applyMatchedProperties<HighPriorityProperties>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size
    if (state.lineHeightValue())
        applyProperty(CSSPropertyLineHeight, state.lineHeightValue());

    // Now do rest of the properties.
    if (keyframe->properties())
        applyMatchedProperties<LowPriorityProperties>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied by one of the non-essential font props,
    // go ahead and update it a second time.
    updateFont();

    // Start loading resources referenced by this style.
    loadPendingResources();

    // Add all the animating properties to the keyframe.
    if (const StylePropertySet* styleDeclaration = keyframe->properties()) {
        unsigned propertyCount = styleDeclaration->propertyCount();
        for (unsigned i = 0; i < propertyCount; ++i) {
            CSSPropertyID property = styleDeclaration->propertyAt(i).id();
            // Timing-function within keyframes is special, because it is not animated; it just
            // describes the timing function between this keyframe and the next.
            if (property != CSSPropertyWebkitAnimationTimingFunction)
                keyframeValue.addProperty(property);
        }
    }

    document()->didAccessStyleResolver();

    return state.takeStyle();
}

void StyleResolver::keyframeStylesForAnimation(Element* e, const RenderStyle* elementStyle, KeyframeList& list)
{
    list.clear();

    // Get the keyframesRule for this name
    if (!e || list.animationName().isEmpty())
        return;

    m_keyframesRuleMap.checkConsistency();

    KeyframesRuleMap::iterator it = m_keyframesRuleMap.find(list.animationName().impl());
    if (it == m_keyframesRuleMap.end())
        return;

    const StyleRuleKeyframes* keyframesRule = it->value.get();

    // Construct and populate the style for each keyframe
    const Vector<RefPtr<StyleKeyframe> >& keyframes = keyframesRule->keyframes();
    for (unsigned i = 0; i < keyframes.size(); ++i) {
        // Apply the declaration to the style. This is a simplified version of the logic in styleForElement
        initElement(e);
        m_state.initForStyleResolve(document(), e);

        const StyleKeyframe* keyframe = keyframes[i].get();

        KeyframeValue keyframeValue(0, 0);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, keyframe, keyframeValue));

        // Add this keyframe style to all the indicated key times
        Vector<float> keys;
        keyframe->getKeys(keys);
        for (size_t keyIndex = 0; keyIndex < keys.size(); ++keyIndex) {
            keyframeValue.setKey(keys[keyIndex]);
            list.insert(keyframeValue);
        }
    }

    // If the 0% keyframe is missing, create it (but only if there is at least one other keyframe)
    int initialListSize = list.size();
    if (initialListSize > 0 && list[0].key()) {
        static StyleKeyframe* zeroPercentKeyframe;
        if (!zeroPercentKeyframe) {
            zeroPercentKeyframe = StyleKeyframe::create().leakRef();
            zeroPercentKeyframe->setKeyText("0%");
        }
        KeyframeValue keyframeValue(0, 0);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, zeroPercentKeyframe, keyframeValue));
        list.insert(keyframeValue);
    }

    // If the 100% keyframe is missing, create it (but only if there is at least one other keyframe)
    if (initialListSize > 0 && (list[list.size() - 1].key() != 1)) {
        static StyleKeyframe* hundredPercentKeyframe;
        if (!hundredPercentKeyframe) {
            hundredPercentKeyframe = StyleKeyframe::create().leakRef();
            hundredPercentKeyframe->setKeyText("100%");
        }
        KeyframeValue keyframeValue(1, 0);
        keyframeValue.setStyle(styleForKeyframe(elementStyle, hundredPercentKeyframe, keyframeValue));
        list.insert(keyframeValue);
    }
}

PassRefPtr<RenderStyle> StyleResolver::pseudoStyleForElement(Element* e, const PseudoStyleRequest& pseudoStyleRequest, RenderStyle* parentStyle)
{
    ASSERT(parentStyle);
    if (!e)
        return 0;

    StyleResolverState& state = m_state;

    initElement(e);

    state.initForStyleResolve(document(), e, parentStyle);

    if (m_state.parentStyle()) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(m_state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }

    // Since we don't use pseudo-elements in any of our quirk/print user agent rules, don't waste time walking
    // those rules.

    // Check UA, user and author rules.
    ElementRuleCollector collector(this, state);
    collector.setPseudoStyleRequest(pseudoStyleRequest);
    matchUARules(collector);

    if (m_matchAuthorAndUserStyles) {
        matchUserRules(collector, false);
        matchAuthorRules(collector, false);
    }

    if (collector.matchedResult().matchedProperties.isEmpty())
        return 0;

    state.style()->setStyleType(pseudoStyleRequest.pseudoId);

    applyMatchedProperties(collector.matchedResult(), e);

    // Clean up our style object's display and text decorations (among other fixups).
    adjustRenderStyle(state.style(), m_state.parentStyle(), 0);

    // Start loading resources referenced by this style.
    loadPendingResources();

    document()->didAccessStyleResolver();

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForPage(int pageIndex)
{
    m_state.initForStyleResolve(document(), document()->documentElement()); // m_rootElementStyle will be set to the document style.

    m_state.setStyle(RenderStyle::create());
    m_state.style()->inheritFrom(m_state.rootElementStyle());

    PageRuleCollector collector(m_state, pageIndex);

    collector.matchPageRules(CSSDefaultStyleSheets::defaultPrintStyle);
    collector.matchPageRules(m_ruleSets.userStyle());

    if (ScopedStyleResolver* scopedResolver = m_styleTree.scopedStyleResolverForDocument())
        scopedResolver->matchPageRules(collector);

    m_state.setLineHeightValue(0);
    bool inheritedOnly = false;

    MatchResult& result = collector.matchedResult();
    applyMatchedProperties<VariableDefinitions>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size.
    if (m_state.lineHeightValue())
        applyProperty(CSSPropertyLineHeight, m_state.lineHeightValue());

    applyMatchedProperties<LowPriorityProperties>(result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // Start loading resources referenced by this style.
    loadPendingResources();

    document()->didAccessStyleResolver();

    // Now return the style.
    return m_state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::defaultStyleForElement()
{
    m_state.setStyle(RenderStyle::create());
    // Make sure our fonts are initialized if we don't inherit them from our parent style.
    if (Settings* settings = documentSettings()) {
        initializeFontStyle(settings);
        m_state.style()->font().update(fontSelector());
    } else
        m_state.style()->font().update(0);

    return m_state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForText(Text* textNode)
{
    ASSERT(textNode);

    NodeRenderingContext context(textNode);
    Node* parentNode = context.parentNodeForRenderingAndStyle();
    return context.resetStyleInheritance() || !parentNode || !parentNode->renderStyle() ?
        defaultStyleForElement() : parentNode->renderStyle();
}

static void addIntrinsicMargins(RenderStyle* style)
{
    // Intrinsic margin value.
    const int intrinsicMargin = 2 * style->effectiveZoom();

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    // FIXME: Using "quirk" to decide the margin wasn't set is kind of lame.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(intrinsicMargin, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(intrinsicMargin, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(intrinsicMargin, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(intrinsicMargin, Fixed));
    }
}

static EDisplay equivalentBlockDisplay(EDisplay display, bool isFloating, bool strictParsing)
{
    switch (display) {
    case BLOCK:
    case TABLE:
    case BOX:
    case FLEX:
    case GRID:
    case LAZY_BLOCK:
        return display;

    case LIST_ITEM:
        // It is a WinIE bug that floated list items lose their bullets, so we'll emulate the quirk, but only in quirks mode.
        if (!strictParsing && isFloating)
            return BLOCK;
        return display;
    case INLINE_TABLE:
        return TABLE;
    case INLINE_BOX:
        return BOX;
    case INLINE_FLEX:
        return FLEX;
    case INLINE_GRID:
        return GRID;

    case INLINE:
    case RUN_IN:
    case COMPACT:
    case INLINE_BLOCK:
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
    case TABLE_ROW:
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
    case TABLE_CELL:
    case TABLE_CAPTION:
        return BLOCK;
    case NONE:
        ASSERT_NOT_REACHED();
        return NONE;
    }
    ASSERT_NOT_REACHED();
    return BLOCK;
}

// CSS requires text-decoration to be reset at each DOM element for tables,
// inline blocks, inline tables, run-ins, shadow DOM crossings, floating elements,
// and absolute or relatively positioned elements.
static bool doesNotInheritTextDecoration(RenderStyle* style, Element* e)
{
    return style->display() == TABLE || style->display() == INLINE_TABLE || style->display() == RUN_IN
        || style->display() == INLINE_BLOCK || style->display() == INLINE_BOX || isAtShadowBoundary(e)
        || style->isFloating() || style->hasOutOfFlowPosition();
}

static bool isDisplayFlexibleBox(EDisplay display)
{
    return display == FLEX || display == INLINE_FLEX;
}

static bool isDisplayGridBox(EDisplay display)
{
    return display == GRID || display == INLINE_GRID;
}

void StyleResolver::adjustRenderStyle(RenderStyle* style, RenderStyle* parentStyle, Element *e)
{
    ASSERT(parentStyle);

    // Cache our original display.
    style->setOriginalDisplay(style->display());

    if (style->display() != NONE) {
        // If we have a <td> that specifies a float property, in quirks mode we just drop the float
        // property.
        // Sites also commonly use display:inline/block on <td>s and <table>s. In quirks mode we force
        // these tags to retain their display types.
        if (document()->inQuirksMode() && e) {
            if (e->hasTagName(tdTag)) {
                style->setDisplay(TABLE_CELL);
                style->setFloating(NoFloat);
            } else if (e->hasTagName(tableTag))
                style->setDisplay(style->isDisplayInlineType() ? INLINE_TABLE : TABLE);
        }

        if (e && (e->hasTagName(tdTag) || e->hasTagName(thTag))) {
            if (style->whiteSpace() == KHTML_NOWRAP) {
                // Figure out if we are really nowrapping or if we should just
                // use normal instead. If the width of the cell is fixed, then
                // we don't actually use NOWRAP.
                if (style->width().isFixed())
                    style->setWhiteSpace(NORMAL);
                else
                    style->setWhiteSpace(NOWRAP);
            }
        }

        // Tables never support the -webkit-* values for text-align and will reset back to the default.
        if (e && e->hasTagName(tableTag) && (style->textAlign() == WEBKIT_LEFT || style->textAlign() == WEBKIT_CENTER || style->textAlign() == WEBKIT_RIGHT))
            style->setTextAlign(TASTART);

        // Frames and framesets never honor position:relative or position:absolute. This is necessary to
        // fix a crash where a site tries to position these objects. They also never honor display.
        if (e && (e->hasTagName(frameTag) || e->hasTagName(framesetTag))) {
            style->setPosition(StaticPosition);
            style->setDisplay(BLOCK);
        }

        // Ruby text does not support float or position. This might change with evolution of the specification.
        if (e && e->hasTagName(rtTag)) {
            style->setPosition(StaticPosition);
            style->setFloating(NoFloat);
        }

        // FIXME: We shouldn't be overriding start/-webkit-auto like this. Do it in html.css instead.
        // Table headers with a text-align of -webkit-auto will change the text-align to center.
        if (e && e->hasTagName(thTag) && style->textAlign() == TASTART)
            style->setTextAlign(CENTER);

        if (e && e->hasTagName(legendTag))
            style->setDisplay(BLOCK);

        // Per the spec, position 'static' and 'relative' in the top layer compute to 'absolute'.
        if (e && e->isInTopLayer() && (style->position() == StaticPosition || style->position() == RelativePosition))
            style->setPosition(AbsolutePosition);

        // Absolute/fixed positioned elements, floating elements and the document element need block-like outside display.
        if (style->hasOutOfFlowPosition() || style->isFloating() || (e && e->document()->documentElement() == e))
            style->setDisplay(equivalentBlockDisplay(style->display(), style->isFloating(), !document()->inQuirksMode()));

        // FIXME: Don't support this mutation for pseudo styles like first-letter or first-line, since it's not completely
        // clear how that should work.
        if (style->display() == INLINE && style->styleType() == NOPSEUDO && style->writingMode() != parentStyle->writingMode())
            style->setDisplay(INLINE_BLOCK);

        // After performing the display mutation, check table rows. We do not honor position:relative or position:sticky on
        // table rows or cells. This has been established for position:relative in CSS2.1 (and caused a crash in containingBlock()
        // on some sites).
        if ((style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_ROW_GROUP
            || style->display() == TABLE_FOOTER_GROUP || style->display() == TABLE_ROW)
            && style->hasInFlowPosition())
            style->setPosition(StaticPosition);

        // writing-mode does not apply to table row groups, table column groups, table rows, and table columns.
        // FIXME: Table cells should be allowed to be perpendicular or flipped with respect to the table, though.
        if (style->display() == TABLE_COLUMN || style->display() == TABLE_COLUMN_GROUP || style->display() == TABLE_FOOTER_GROUP
            || style->display() == TABLE_HEADER_GROUP || style->display() == TABLE_ROW || style->display() == TABLE_ROW_GROUP
            || style->display() == TABLE_CELL)
            style->setWritingMode(parentStyle->writingMode());

        // FIXME: Since we don't support block-flow on flexible boxes yet, disallow setting
        // of block-flow to anything other than TopToBottomWritingMode.
        // https://bugs.webkit.org/show_bug.cgi?id=46418 - Flexible box support.
        if (style->writingMode() != TopToBottomWritingMode && (style->display() == BOX || style->display() == INLINE_BOX))
            style->setWritingMode(TopToBottomWritingMode);

        if (isDisplayFlexibleBox(parentStyle->display()) || isDisplayGridBox(parentStyle->display())) {
            style->setFloating(NoFloat);
            style->setDisplay(equivalentBlockDisplay(style->display(), style->isFloating(), !document()->inQuirksMode()));
        }
    }

    // Make sure our z-index value is only applied if the object is positioned.
    if (style->position() == StaticPosition && !isDisplayFlexibleBox(parentStyle->display()))
        style->setHasAutoZIndex();

    // Auto z-index becomes 0 for the root element and transparent objects. This prevents
    // cases where objects that should be blended as a single unit end up with a non-transparent
    // object wedged in between them. Auto z-index also becomes 0 for objects that specify transforms/masks/reflections.
    if (style->hasAutoZIndex() && ((e && e->document()->documentElement() == e)
        || style->opacity() < 1.0f
        || style->hasTransformRelatedProperty()
        || style->hasMask()
        || style->clipPath()
        || style->boxReflect()
        || style->hasFilter()
        || style->hasBlendMode()
        || style->position() == StickyPosition
        || (style->position() == FixedPosition && e && e->document()->page() && e->document()->page()->settings()->fixedPositionCreatesStackingContext())
        || (e && e->isInTopLayer())
        ))
        style->setZIndex(0);

    // Textarea considers overflow visible as auto.
    if (e && e->hasTagName(textareaTag)) {
        style->setOverflowX(style->overflowX() == OVISIBLE ? OAUTO : style->overflowX());
        style->setOverflowY(style->overflowY() == OVISIBLE ? OAUTO : style->overflowY());
    }

    // For now, <marquee> requires an overflow clip to work properly.
    if (e && e->hasTagName(marqueeTag)) {
        style->setOverflowX(OHIDDEN);
        style->setOverflowY(OHIDDEN);
    }

    if (doesNotInheritTextDecoration(style, e))
        style->setTextDecorationsInEffect(style->textDecoration());
    else
        style->addToTextDecorationsInEffect(style->textDecoration());

    // If either overflow value is not visible, change to auto.
    if (style->overflowX() == OVISIBLE && style->overflowY() != OVISIBLE) {
        // FIXME: Once we implement pagination controls, overflow-x should default to hidden
        // if overflow-y is set to -webkit-paged-x or -webkit-page-y. For now, we'll let it
        // default to auto so we can at least scroll through the pages.
        style->setOverflowX(OAUTO);
    } else if (style->overflowY() == OVISIBLE && style->overflowX() != OVISIBLE)
        style->setOverflowY(OAUTO);

    // Call setStylesForPaginationMode() if a pagination mode is set for any non-root elements. If these
    // styles are specified on a root element, then they will be incorporated in
    // StyleResolver::styleForDocument().
    if ((style->overflowY() == OPAGEDX || style->overflowY() == OPAGEDY) && !(e && (e->hasTagName(htmlTag) || e->hasTagName(bodyTag))))
        setStylesForPaginationMode(WebCore::paginationModeForRenderStyle(style), style);

    // Table rows, sections and the table itself will support overflow:hidden and will ignore scroll/auto.
    // FIXME: Eventually table sections will support auto and scroll.
    if (style->display() == TABLE || style->display() == INLINE_TABLE
        || style->display() == TABLE_ROW_GROUP || style->display() == TABLE_ROW) {
        if (style->overflowX() != OVISIBLE && style->overflowX() != OHIDDEN)
            style->setOverflowX(OVISIBLE);
        if (style->overflowY() != OVISIBLE && style->overflowY() != OHIDDEN)
            style->setOverflowY(OVISIBLE);
    }

    // Menulists should have visible overflow
    if (style->appearance() == MenulistPart) {
        style->setOverflowX(OVISIBLE);
        style->setOverflowY(OVISIBLE);
    }

    // Cull out any useless layers and also repeat patterns into additional layers.
    style->adjustBackgroundLayers();
    style->adjustMaskLayers();

    // Do the same for animations and transitions.
    style->adjustAnimations();
    style->adjustTransitions();

    // Important: Intrinsic margins get added to controls before the theme has adjusted the style, since the theme will
    // alter fonts and heights/widths.
    if (e && e->isFormControlElement() && style->fontSize() >= 11) {
        // Don't apply intrinsic margins to image buttons. The designer knows how big the images are,
        // so we have to treat all image buttons as though they were explicitly sized.
        if (!e->hasTagName(inputTag) || !toHTMLInputElement(e)->isImageButton())
            addIntrinsicMargins(style);
    }

    // Let the theme also have a crack at adjusting the style.
    if (style->hasAppearance())
        RenderTheme::defaultTheme()->adjustStyle(style, e, m_state.hasUAAppearance(), m_state.borderData(), m_state.backgroundData(), m_state.backgroundColor());

    // If we have first-letter pseudo style, do not share this style.
    if (style->hasPseudoStyle(FIRST_LETTER))
        style->setUnique();

    // FIXME: when dropping the -webkit prefix on transform-style, we should also have opacity < 1 cause flattening.
    if (style->preserves3D() && (style->overflowX() != OVISIBLE
        || style->overflowY() != OVISIBLE
        || style->hasFilter()))
        style->setTransformStyle3D(TransformStyle3DFlat);

    // Seamless iframes behave like blocks. Map their display to inline-block when marked inline.
    if (e && e->hasTagName(iframeTag) && style->display() == INLINE && static_cast<HTMLIFrameElement*>(e)->shouldDisplaySeamlessly())
        style->setDisplay(INLINE_BLOCK);

    adjustGridItemPosition(style);

    if (e && e->isSVGElement()) {
        // Spec: http://www.w3.org/TR/SVG/masking.html#OverflowProperty
        if (style->overflowY() == OSCROLL)
            style->setOverflowY(OHIDDEN);
        else if (style->overflowY() == OAUTO)
            style->setOverflowY(OVISIBLE);

        if (style->overflowX() == OSCROLL)
            style->setOverflowX(OHIDDEN);
        else if (style->overflowX() == OAUTO)
            style->setOverflowX(OVISIBLE);

        // Only the root <svg> element in an SVG document fragment tree honors css position
        if (!(e->hasTagName(SVGNames::svgTag) && e->parentNode() && !e->parentNode()->isSVGElement()))
            style->setPosition(RenderStyle::initialPosition());

        // RenderSVGRoot handles zooming for the whole SVG subtree, so foreignObject content should
        // not be scaled again.
        if (e->hasTagName(SVGNames::foreignObjectTag))
            style->setEffectiveZoom(RenderStyle::initialZoom());
    }
}

void StyleResolver::adjustGridItemPosition(RenderStyle* style) const
{
    // If opposing grid-placement properties both specify a grid span, they both compute to ‘auto’.
    if (style->gridStart().isSpan() && style->gridEnd().isSpan()) {
        style->setGridStart(GridPosition());
        style->setGridEnd(GridPosition());
    }

    if (style->gridBefore().isSpan() && style->gridAfter().isSpan()) {
        style->setGridBefore(GridPosition());
        style->setGridAfter(GridPosition());
    }
}

bool StyleResolver::checkRegionStyle(Element* regionElement)
{
    // FIXME (BUG 72472): We don't add @-webkit-region rules of scoped style sheets for the moment,
    // so all region rules are global by default. Verify whether that can stand or needs changing.

    if (ScopedStyleResolver* scopedResolver = m_styleTree.scopedStyleResolverForDocument())
        if (scopedResolver->checkRegionStyle(regionElement))
            return true;

    if (m_ruleSets.userStyle()) {
        unsigned rulesSize = m_ruleSets.userStyle()->m_regionSelectorsAndRuleSets.size();
        for (unsigned i = 0; i < rulesSize; ++i) {
            ASSERT(m_ruleSets.userStyle()->m_regionSelectorsAndRuleSets.at(i).ruleSet.get());
            if (checkRegionSelector(m_ruleSets.userStyle()->m_regionSelectorsAndRuleSets.at(i).selector, regionElement))
                return true;
        }
    }

    return false;
}

static void checkForOrientationChange(RenderStyle* style)
{
    FontOrientation fontOrientation;
    NonCJKGlyphOrientation glyphOrientation;
    getFontAndGlyphOrientation(style, fontOrientation, glyphOrientation);

    const FontDescription& fontDescription = style->fontDescription();
    if (fontDescription.orientation() == fontOrientation && fontDescription.nonCJKGlyphOrientation() == glyphOrientation)
        return;

    FontDescription newFontDescription(fontDescription);
    newFontDescription.setNonCJKGlyphOrientation(glyphOrientation);
    newFontDescription.setOrientation(fontOrientation);
    style->setFontDescription(newFontDescription);
}

void StyleResolver::updateFont()
{
    if (!m_state.fontDirty())
        return;

    RenderStyle* style = m_state.style();
    checkForGenericFamilyChange(style, m_state.parentStyle());
    checkForZoomChange(style, m_state.parentStyle());
    checkForOrientationChange(style);
    style->font().update(m_fontSelector);
    m_state.setFontDirty(false);
}

PassRefPtr<CSSRuleList> StyleResolver::styleRulesForElement(Element* e, unsigned rulesToInclude)
{
    return pseudoStyleRulesForElement(e, NOPSEUDO, rulesToInclude);
}

PassRefPtr<CSSRuleList> StyleResolver::pseudoStyleRulesForElement(Element* e, PseudoId pseudoId, unsigned rulesToInclude)
{
    if (!e || !e->document()->haveStylesheetsLoaded())
        return 0;

    initElement(e);
    m_state.initForStyleResolve(document(), e, 0);

    ElementRuleCollector collector(this, m_state);
    collector.setMode(SelectorChecker::CollectingRules);
    collector.setPseudoStyleRequest(PseudoStyleRequest(pseudoId));

    if (rulesToInclude & UAAndUserCSSRules) {
        // First we match rules from the user agent sheet.
        matchUARules(collector);

        // Now we check user sheet rules.
        if (m_matchAuthorAndUserStyles)
            matchUserRules(collector, rulesToInclude & EmptyCSSRules);
    }

    if (m_matchAuthorAndUserStyles && (rulesToInclude & AuthorCSSRules)) {
        collector.setSameOriginOnly(!(rulesToInclude & CrossOriginCSSRules));

        // Check the rules in author sheets.
        matchAuthorRules(collector, rulesToInclude & EmptyCSSRules);
    }

    return collector.matchedRuleList();
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

Length StyleResolver::convertToIntLength(CSSPrimitiveValue* primitiveValue, RenderStyle* style, RenderStyle* rootStyle, double multiplier)
{
    return primitiveValue ? primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion | FractionConversion | ViewportPercentageConversion>(style, rootStyle, multiplier) : Length(Undefined);
}

Length StyleResolver::convertToFloatLength(CSSPrimitiveValue* primitiveValue, RenderStyle* style, RenderStyle* rootStyle, double multiplier)
{
    return primitiveValue ? primitiveValue->convertToLength<FixedFloatConversion | PercentConversion | CalculatedConversion | FractionConversion | ViewportPercentageConversion>(style, rootStyle, multiplier) : Length(Undefined);
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyAnimatedProperties(const Element* target)
{
    if (!target->hasActiveAnimations())
        return;

    Vector<Animation*>* animations = target->activeAnimations();

    for (size_t i = 0; i < animations->size(); ++i) {
        RefPtr<Animation> animation = animations->at(i);
        const AnimationEffect::CompositableValueMap* compositableValues = animation->compositableValues();
        for (AnimationEffect::CompositableValueMap::const_iterator iter = compositableValues->begin(); iter != compositableValues->end(); ++iter) {
            CSSPropertyID property = iter->key;
            // FIXME: Composite onto the underlying value.
            RefPtr<CSSValue> value = iter->value->composite(AnimatableValue()).toCSSValue();
            switch (pass) {
            case VariableDefinitions:
                ASSERT_NOT_REACHED();
                continue;
            case HighPriorityProperties:
                if (property < CSSPropertyLineHeight)
                    applyProperty(property, value.get());
                else if (property == CSSPropertyLineHeight)
                    m_state.setLineHeightValue(value.get());
                continue;
            case LowPriorityProperties:
                if (property > CSSPropertyLineHeight)
                    applyProperty(property, value.get());
                continue;
            }
        }
    }
}

static inline bool isValidVisitedLinkProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyColor:
    case CSSPropertyFill:
    case CSSPropertyOutlineColor:
    case CSSPropertyStroke:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
        return true;
    default:
        break;
    }

    return false;
}

// http://dev.w3.org/csswg/css3-regions/#the-at-region-style-rule
// FIXME: add incremental support for other region styling properties.
static inline bool isValidRegionStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackgroundColor:
    case CSSPropertyColor:
        return true;
    default:
        break;
    }

    return false;
}

static inline bool isValidCueStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackground:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyColor:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyLineHeight:
    case CSSPropertyOpacity:
    case CSSPropertyOutline:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyVisibility:
    case CSSPropertyWhiteSpace:
    case CSSPropertyTextDecoration:
    case CSSPropertyTextShadow:
    case CSSPropertyBorderStyle:
        return true;
    default:
        break;
    }
    return false;
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyProperties(const StylePropertySet* properties, StyleRule* rule, bool isImportant, bool inheritedOnly, PropertyWhitelistType propertyWhitelistType)
{
    ASSERT((propertyWhitelistType != PropertyWhitelistRegion) || m_state.regionForStyling());
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willProcessRule(document(), rule, this);

    unsigned propertyCount = properties->propertyCount();
    for (unsigned i = 0; i < propertyCount; ++i) {
        StylePropertySet::PropertyReference current = properties->propertyAt(i);
        if (isImportant != current.isImportant())
            continue;
        if (inheritedOnly && !current.isInherited()) {
            // If the property value is explicitly inherited, we need to apply further non-inherited properties
            // as they might override the value inherited here. For this reason we don't allow declarations with
            // explicitly inherited properties to be cached.
            ASSERT(!current.value()->isInheritedValue());
            continue;
        }
        CSSPropertyID property = current.id();

        if (propertyWhitelistType == PropertyWhitelistRegion && !isValidRegionStyleProperty(property))
            continue;
        if (propertyWhitelistType == PropertyWhitelistCue && !isValidCueStyleProperty(property))
            continue;
        switch (pass) {
        case VariableDefinitions:
            COMPILE_ASSERT(CSSPropertyVariable < firstCSSProperty, CSS_variable_is_before_first_property);
            if (property == CSSPropertyVariable)
                applyProperty(current.id(), current.value());
            break;
        case HighPriorityProperties:
            COMPILE_ASSERT(firstCSSProperty == CSSPropertyColor, CSS_color_is_first_property);
            COMPILE_ASSERT(CSSPropertyZoom == CSSPropertyColor + 17, CSS_zoom_is_end_of_first_prop_range);
            COMPILE_ASSERT(CSSPropertyLineHeight == CSSPropertyZoom + 1, CSS_line_height_is_after_zoom);
            if (property == CSSPropertyVariable)
                continue;
            // give special priority to font-xxx, color properties, etc
            if (property < CSSPropertyLineHeight)
                applyProperty(current.id(), current.value());
            // we apply line-height later
            else if (property == CSSPropertyLineHeight)
                m_state.setLineHeightValue(current.value());
            break;
        case LowPriorityProperties:
            if (property > CSSPropertyLineHeight)
                applyProperty(current.id(), current.value());
        }
    }
    InspectorInstrumentation::didProcessRule(cookie);
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyMatchedProperties(const MatchResult& matchResult, bool isImportant, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return;

    StyleResolverState& state = m_state;
    if (state.style()->insideLink() != NotInsideLink) {
        for (int i = startIndex; i <= endIndex; ++i) {
            const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
            unsigned linkMatchType = matchedProperties.linkMatchType;
            // FIXME: It would be nicer to pass these as arguments but that requires changes in many places.
            state.setApplyPropertyToRegularStyle(linkMatchType & SelectorChecker::MatchLink);
            state.setApplyPropertyToVisitedLinkStyle(linkMatchType & SelectorChecker::MatchVisited);

            applyProperties<pass>(matchedProperties.properties.get(), matchResult.matchedRules[i], isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.whitelistType));
        }
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        return;
    }
    for (int i = startIndex; i <= endIndex; ++i) {
        const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
        applyProperties<pass>(matchedProperties.properties.get(), matchResult.matchedRules[i], isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.whitelistType));
    }
}

static unsigned computeMatchedPropertiesHash(const StyleResolver::MatchedProperties* properties, unsigned size)
{
    return StringHasher::hashMemory(properties, sizeof(StyleResolver::MatchedProperties) * size);
}

bool operator==(const StyleResolver::MatchRanges& a, const StyleResolver::MatchRanges& b)
{
    return a.firstUARule == b.firstUARule
        && a.lastUARule == b.lastUARule
        && a.firstAuthorRule == b.firstAuthorRule
        && a.lastAuthorRule == b.lastAuthorRule
        && a.firstUserRule == b.firstUserRule
        && a.lastUserRule == b.lastUserRule;
}

bool operator!=(const StyleResolver::MatchRanges& a, const StyleResolver::MatchRanges& b)
{
    return !(a == b);
}

bool operator==(const StyleResolver::MatchedProperties& a, const StyleResolver::MatchedProperties& b)
{
    return a.properties == b.properties && a.linkMatchType == b.linkMatchType;
}

bool operator!=(const StyleResolver::MatchedProperties& a, const StyleResolver::MatchedProperties& b)
{
    return !(a == b);
}

const StyleResolver::MatchedPropertiesCacheItem* StyleResolver::findFromMatchedPropertiesCache(unsigned hash, const MatchResult& matchResult)
{
    ASSERT(hash);

    MatchedPropertiesCache::iterator it = m_matchedPropertiesCache.find(hash);
    if (it == m_matchedPropertiesCache.end())
        return 0;
    MatchedPropertiesCacheItem& cacheItem = it->value;

    size_t size = matchResult.matchedProperties.size();
    if (size != cacheItem.matchedProperties.size())
        return 0;
    if (cacheItem.renderStyle->insideLink() != m_state.style()->insideLink())
        return 0;
    for (size_t i = 0; i < size; ++i) {
        if (matchResult.matchedProperties[i] != cacheItem.matchedProperties[i])
            return 0;
    }
    if (cacheItem.ranges != matchResult.ranges)
        return 0;
    return &cacheItem;
}

void StyleResolver::addToMatchedPropertiesCache(const RenderStyle* style, const RenderStyle* parentStyle, unsigned hash, const MatchResult& matchResult)
{
    static const unsigned matchedDeclarationCacheAdditionsBetweenSweeps = 100;
    if (++m_matchedPropertiesCacheAdditionsSinceLastSweep >= matchedDeclarationCacheAdditionsBetweenSweeps
        && !m_matchedPropertiesCacheSweepTimer.isActive()) {
        static const unsigned matchedDeclarationCacheSweepTimeInSeconds = 60;
        m_matchedPropertiesCacheSweepTimer.startOneShot(matchedDeclarationCacheSweepTimeInSeconds);
    }

    ASSERT(hash);
    MatchedPropertiesCacheItem cacheItem;
    cacheItem.matchedProperties.append(matchResult.matchedProperties);
    cacheItem.ranges = matchResult.ranges;
    // Note that we don't cache the original RenderStyle instance. It may be further modified.
    // The RenderStyle in the cache is really just a holder for the substructures and never used as-is.
    cacheItem.renderStyle = RenderStyle::clone(style);
    cacheItem.parentRenderStyle = RenderStyle::clone(parentStyle);
    m_matchedPropertiesCache.add(hash, cacheItem);
}

void StyleResolver::invalidateMatchedPropertiesCache()
{
    m_matchedPropertiesCache.clear();
}

static bool isCacheableInMatchedPropertiesCache(const Element* element, const RenderStyle* style, const RenderStyle* parentStyle)
{
    // FIXME: CSSPropertyWebkitWritingMode modifies state when applying to document element. We can't skip the applying by caching.
    if (element == element->document()->documentElement() && element->document()->writingModeSetOnDocumentElement())
        return false;
    if (style->unique() || (style->styleType() != NOPSEUDO && parentStyle->unique()))
        return false;
    if (style->hasAppearance())
        return false;
    if (style->zoom() != RenderStyle::initialZoom())
        return false;
    if (style->writingMode() != RenderStyle::initialWritingMode())
        return false;
    // The cache assumes static knowledge about which properties are inherited.
    if (parentStyle->hasExplicitlyInheritedProperties())
        return false;
    return true;
}

void StyleResolver::applyMatchedProperties(const MatchResult& matchResult, const Element* element)
{
    ASSERT(element);
    StyleResolverState& state = m_state;
    unsigned cacheHash = matchResult.isCacheable ? computeMatchedPropertiesHash(matchResult.matchedProperties.data(), matchResult.matchedProperties.size()) : 0;
    bool applyInheritedOnly = false;
    const MatchedPropertiesCacheItem* cacheItem = 0;
    if (cacheHash && (cacheItem = findFromMatchedPropertiesCache(cacheHash, matchResult))) {
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the
        // element context. This is fast and saves memory by reusing the style data structures.
        state.style()->copyNonInheritedFrom(cacheItem->renderStyle.get());
        if (state.parentStyle()->inheritedDataShared(cacheItem->parentRenderStyle.get()) && !isAtShadowBoundary(element)) {
            EInsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            state.style()->inheritFrom(cacheItem->renderStyle.get());

            // Unfortunately the link status is treated like an inherited property. We need to explicitly restore it.
            state.style()->setInsideLink(linkStatus);
            return;
        }
        applyInheritedOnly = true;
    }

    // First apply all variable definitions, as they may be used during application of later properties.
    applyMatchedProperties<VariableDefinitions>(matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<VariableDefinitions>(matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<VariableDefinitions>(matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<VariableDefinitions>(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Now we have all of the matched rules in the appropriate order. Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    state.setLineHeightValue(0);
    applyMatchedProperties<HighPriorityProperties>(matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    // Animation contributions are processed here because CSS Animations are overridable by user !important rules.
    if (RuntimeEnabledFeatures::webAnimationsEnabled())
        applyAnimatedProperties<HighPriorityProperties>(element);
    applyMatchedProperties<HighPriorityProperties>(matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    if (cacheItem && cacheItem->renderStyle->effectiveZoom() != state.style()->effectiveZoom()) {
        state.setFontDirty(true);
        applyInheritedOnly = false;
    }

    // If our font got dirtied, go ahead and update it now.
    updateFont();

    // Line-height is set when we are sure we decided on the font-size.
    if (state.lineHeightValue())
        applyProperty(CSSPropertyLineHeight, state.lineHeightValue());

    // Many properties depend on the font. If it changes we just apply all properties.
    if (cacheItem && cacheItem->renderStyle->fontDescription() != state.style()->fontDescription())
        applyInheritedOnly = false;

    // Now do the normal priority UA properties.
    applyMatchedProperties<LowPriorityProperties>(matchResult, false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Cache our border and background so that we can examine them later.
    state.cacheBorderAndBackground();

    // Now do the author and user normal priority properties and all the !important properties.
    applyMatchedProperties<LowPriorityProperties>(matchResult, false, matchResult.ranges.lastUARule + 1, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    if (RuntimeEnabledFeatures::webAnimationsEnabled())
        applyAnimatedProperties<LowPriorityProperties>(element);
    applyMatchedProperties<LowPriorityProperties>(matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Start loading resources referenced by this style.
    loadPendingResources();

    ASSERT(!state.fontDirty());

    if (cacheItem || !cacheHash)
        return;
    if (!isCacheableInMatchedPropertiesCache(state.element(), state.style(), state.parentStyle()))
        return;
    addToMatchedPropertiesCache(state.style(), state.parentStyle(), cacheHash, matchResult);
}

void StyleResolver::applyPropertyToStyle(CSSPropertyID id, CSSValue* value, RenderStyle* style)
{
    initElement(0);
    m_state.initForStyleResolve(document(), 0, style);
    m_state.setStyle(style);
    applyPropertyToCurrentStyle(id, value);
}

void StyleResolver::applyPropertyToCurrentStyle(CSSPropertyID id, CSSValue* value)
{
    if (value)
        applyProperty(id, value);
}

// SVG handles zooming in a different way compared to CSS. The whole document is scaled instead
// of each individual length value in the render style / tree. CSSPrimitiveValue::computeLength*()
// multiplies each resolved length with the zoom multiplier - so for SVG we need to disable that.
// Though all CSS values that can be applied to outermost <svg> elements (width/height/border/padding...)
// need to respect the scaling. RenderBox (the parent class of RenderSVGRoot) grabs values like
// width/height/border/padding/... from the RenderStyle -> for SVG these values would never scale,
// if we'd pass a 1.0 zoom factor everyhwere. So we only pass a zoom factor of 1.0 for specific
// properties that are NOT allowed to scale within a zoomed SVG document (letter/word-spacing/font-size).
bool StyleResolver::useSVGZoomRules()
{
    return m_state.element() && m_state.element()->isSVGElement();
}

static bool createGridTrackBreadth(CSSPrimitiveValue* primitiveValue, const StyleResolverState& state, GridLength& workingLength)
{
    if (primitiveValue->getValueID() == CSSValueWebkitMinContent) {
        workingLength = Length(MinContent);
        return true;
    }

    if (primitiveValue->getValueID() == CSSValueWebkitMaxContent) {
        workingLength = Length(MaxContent);
        return true;
    }

    if (primitiveValue->isFlex()) {
        // Fractional unit.
        workingLength.setFlex(primitiveValue->getFloatValue());
        return true;
    }

    workingLength = primitiveValue->convertToLength<FixedIntegerConversion | PercentConversion | ViewportPercentageConversion | AutoConversion>(state.style(), state.rootElementStyle(), state.style()->effectiveZoom());
    if (workingLength.length().isUndefined())
        return false;

    if (primitiveValue->isLength())
        workingLength.length().setQuirk(primitiveValue->isQuirkValue());

    return true;
}

static bool createGridTrackSize(CSSValue* value, GridTrackSize& trackSize, const StyleResolverState& state)
{
    if (!value->isPrimitiveValue())
        return false;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    Pair* minMaxTrackBreadth = primitiveValue->getPairValue();
    if (!minMaxTrackBreadth) {
        GridLength workingLength;
        if (!createGridTrackBreadth(primitiveValue, state, workingLength))
            return false;

        trackSize.setLength(workingLength);
        return true;
    }

    GridLength minTrackBreadth;
    GridLength maxTrackBreadth;
    if (!createGridTrackBreadth(minMaxTrackBreadth->first(), state, minTrackBreadth) || !createGridTrackBreadth(minMaxTrackBreadth->second(), state, maxTrackBreadth))
        return false;

    trackSize.setMinMax(minTrackBreadth, maxTrackBreadth);
    return true;
}

static bool createGridTrackList(CSSValue* value, Vector<GridTrackSize>& trackSizes, NamedGridLinesMap& namedGridLines, const StyleResolverState& state)
{
    // Handle 'none'.
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        return primitiveValue->getValueID() == CSSValueNone;
    }

    if (!value->isValueList())
        return false;

    size_t currentNamedGridLine = 0;
    for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
        CSSValue* currValue = i.value();
        if (currValue->isPrimitiveValue()) {
            CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(currValue);
            if (primitiveValue->isString()) {
                NamedGridLinesMap::AddResult result = namedGridLines.add(primitiveValue->getStringValue(), Vector<size_t>());
                result.iterator->value.append(currentNamedGridLine);
                continue;
            }
        }

        ++currentNamedGridLine;
        GridTrackSize trackSize;
        if (!createGridTrackSize(currValue, trackSize, state))
            return false;

        trackSizes.append(trackSize);
    }

    // The parser should have rejected any <track-list> without any <track-size> as
    // this is not conformant to the syntax.
    ASSERT(!trackSizes.isEmpty());
    return true;
}


static bool createGridPosition(CSSValue* value, GridPosition& position)
{
    // For now, we only accept: 'auto' | [ <integer> || <string> ] | span && <integer>?

    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        ASSERT(primitiveValue->getValueID() == CSSValueAuto);
        return true;
    }

    CSSValueList* values = toCSSValueList(value);
    ASSERT(values->length());

    bool isSpanPosition = false;
    // The specification makes the <integer> optional, in which case it default to '1'.
    int gridLineNumber = 1;
    String gridLineName;

    CSSValueListIterator it = values;
    CSSPrimitiveValue* currentValue = toCSSPrimitiveValue(it.value());
    if (currentValue->getValueID() == CSSValueSpan) {
        isSpanPosition = true;
        it.advance();
        currentValue = it.hasMore() ? toCSSPrimitiveValue(it.value()) : 0;
    }

    if (currentValue && currentValue->isNumber()) {
        gridLineNumber = currentValue->getIntValue();
        it.advance();
        currentValue = it.hasMore() ? toCSSPrimitiveValue(it.value()) : 0;
    }

    if (currentValue && currentValue->isString()) {
        gridLineName = currentValue->getStringValue();
        it.advance();
    }

    ASSERT(!it.hasMore());
    if (isSpanPosition)
        position.setSpanPosition(gridLineNumber, gridLineName);
    else
        position.setExplicitPosition(gridLineNumber, gridLineName);

    return true;
}

static bool hasVariableReference(CSSValue* value)
{
    if (value->isPrimitiveValue()) {
        CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
        return primitiveValue->hasVariableReference();
    }

    if (value->isCalculationValue())
        return static_cast<CSSCalcValue*>(value)->hasVariableReference();

    if (value->isReflectValue()) {
        CSSReflectValue* reflectValue = static_cast<CSSReflectValue*>(value);
        CSSPrimitiveValue* direction = reflectValue->direction();
        CSSPrimitiveValue* offset = reflectValue->offset();
        CSSValue* mask = reflectValue->mask();
        return (direction && hasVariableReference(direction)) || (offset && hasVariableReference(offset)) || (mask && hasVariableReference(mask));
    }

    for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
        if (hasVariableReference(i.value()))
            return true;
    }

    return false;
}

void StyleResolver::resolveVariables(CSSPropertyID id, CSSValue* value, Vector<std::pair<CSSPropertyID, String> >& knownExpressions)
{
    std::pair<CSSPropertyID, String> expression(id, value->serializeResolvingVariables(*m_state.style()->variables()));

    if (knownExpressions.contains(expression))
        return; // cycle detected.

    knownExpressions.append(expression);

    // FIXME: It would be faster not to re-parse from strings, but for now CSS property validation lives inside the parser so we do it there.
    RefPtr<MutableStylePropertySet> resultSet = MutableStylePropertySet::create();
    if (!CSSParser::parseValue(resultSet.get(), id, expression.second, false, document()))
        return; // expression failed to parse.

    for (unsigned i = 0; i < resultSet->propertyCount(); i++) {
        StylePropertySet::PropertyReference property = resultSet->propertyAt(i);
        if (property.id() != CSSPropertyVariable && hasVariableReference(property.value()))
            resolveVariables(property.id(), property.value(), knownExpressions);
        else
            applyProperty(property.id(), property.value());
    }
}

void StyleResolver::applyProperty(CSSPropertyID id, CSSValue* value)
{
    if (id != CSSPropertyVariable && hasVariableReference(value)) {
        Vector<std::pair<CSSPropertyID, String> > knownExpressions;
        resolveVariables(id, value, knownExpressions);
        return;
    }

    // CSS variables don't resolve shorthands at parsing time, so this should be *after* handling variables.
    ASSERT_WITH_MESSAGE(!isExpandedShorthand(id), "Shorthand property id = %d wasn't expanded at parsing time", id);

    StyleResolverState& state = m_state;
    bool isInherit = state.parentNode() && value->isInheritedValue();
    bool isInitial = value->isInitialValue() || (!state.parentNode() && value->isInheritedValue());

    ASSERT(!isInherit || !isInitial); // isInherit -> !isInitial && isInitial -> !isInherit
    ASSERT(!isInherit || (state.parentNode() && state.parentStyle())); // isInherit -> (state.parentNode() && state.parentStyle())

    if (!state.applyPropertyToRegularStyle() && (!state.applyPropertyToVisitedLinkStyle() || !isValidVisitedLinkProperty(id))) {
        // Limit the properties that can be applied to only the ones honored by :visited.
        return;
    }

    if (isInherit && !state.parentStyle()->hasExplicitlyInheritedProperties() && !CSSProperty::isInheritedProperty(id))
        state.parentStyle()->setHasExplicitlyInheritedProperties();

    if (id == CSSPropertyVariable) {
        ASSERT_WITH_SECURITY_IMPLICATION(value->isVariableValue());
        CSSVariableValue* variable = static_cast<CSSVariableValue*>(value);
        ASSERT(!variable->name().isEmpty());
        ASSERT(!variable->value().isEmpty());
        state.style()->setVariable(variable->name(), variable->value());
        return;
    }

    // Check lookup table for implementations and use when available.
    const PropertyHandler& handler = m_styleBuilder.propertyHandler(id);
    if (handler.isValid()) {
        if (isInherit)
            handler.applyInheritValue(id, this);
        else if (isInitial)
            handler.applyInitialValue(id, this);
        else
            handler.applyValue(id, this, value);
        return;
    }

    // Use the new StyleBuilder.
    if (StyleBuilder::applyProperty(id, this, value, isInitial, isInherit))
        return;

    CSSPrimitiveValue* primitiveValue = value->isPrimitiveValue() ? toCSSPrimitiveValue(value) : 0;

    float zoomFactor = state.style()->effectiveZoom();

    // What follows is a list that maps the CSS properties into their corresponding front-end
    // RenderStyle values.
    switch (id) {
    // lists
    case CSSPropertyContent:
        // list of string, uri, counter, attr, i
        {
            // FIXME: In CSS3, it will be possible to inherit content. In CSS2 it is not. This
            // note is a reminder that eventually "inherit" needs to be supported.

            if (isInitial) {
                state.style()->clearContent();
                return;
            }

            if (!value->isValueList())
                return;

            bool didSet = false;
            for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
                CSSValue* item = i.value();
                if (item->isImageGeneratorValue()) {
                    if (item->isGradientValue())
                        state.style()->setContent(StyleGeneratedImage::create(static_cast<CSSGradientValue*>(item)->gradientWithStylesResolved(this).get()), didSet);
                    else
                        state.style()->setContent(StyleGeneratedImage::create(static_cast<CSSImageGeneratorValue*>(item)), didSet);
                    didSet = true;
                } else if (item->isImageSetValue()) {
                    state.style()->setContent(setOrPendingFromValue(CSSPropertyContent, static_cast<CSSImageSetValue*>(item)), didSet);
                    didSet = true;
                }

                if (item->isImageValue()) {
                    state.style()->setContent(cachedOrPendingFromValue(CSSPropertyContent, toCSSImageValue(item)), didSet);
                    didSet = true;
                    continue;
                }

                if (!item->isPrimitiveValue())
                    continue;

                CSSPrimitiveValue* contentValue = toCSSPrimitiveValue(item);

                if (contentValue->isString()) {
                    state.style()->setContent(contentValue->getStringValue().impl(), didSet);
                    didSet = true;
                } else if (contentValue->isAttr()) {
                    // FIXME: Can a namespace be specified for an attr(foo)?
                    if (state.style()->styleType() == NOPSEUDO)
                        state.style()->setUnique();
                    else
                        state.parentStyle()->setUnique();
                    QualifiedName attr(nullAtom, contentValue->getStringValue().impl(), nullAtom);
                    const AtomicString& value = state.element()->getAttribute(attr);
                    state.style()->setContent(value.isNull() ? emptyAtom : value.impl(), didSet);
                    didSet = true;
                    // register the fact that the attribute value affects the style
                    m_features.attrsInRules.add(attr.localName().impl());
                } else if (contentValue->isCounter()) {
                    Counter* counterValue = contentValue->getCounterValue();
                    EListStyleType listStyleType = NoneListStyle;
                    CSSValueID listStyleIdent = counterValue->listStyleIdent();
                    if (listStyleIdent != CSSValueNone)
                        listStyleType = static_cast<EListStyleType>(listStyleIdent - CSSValueDisc);
                    OwnPtr<CounterContent> counter = adoptPtr(new CounterContent(counterValue->identifier(), listStyleType, counterValue->separator()));
                    state.style()->setContent(counter.release(), didSet);
                    didSet = true;
                } else {
                    switch (contentValue->getValueID()) {
                    case CSSValueOpenQuote:
                        state.style()->setContent(OPEN_QUOTE, didSet);
                        didSet = true;
                        break;
                    case CSSValueCloseQuote:
                        state.style()->setContent(CLOSE_QUOTE, didSet);
                        didSet = true;
                        break;
                    case CSSValueNoOpenQuote:
                        state.style()->setContent(NO_OPEN_QUOTE, didSet);
                        didSet = true;
                        break;
                    case CSSValueNoCloseQuote:
                        state.style()->setContent(NO_CLOSE_QUOTE, didSet);
                        didSet = true;
                        break;
                    default:
                        // normal and none do not have any effect.
                        { }
                    }
                }
            }
            if (!didSet)
                state.style()->clearContent();
            return;
        }
    case CSSPropertyQuotes:
        if (isInherit) {
            state.style()->setQuotes(state.parentStyle()->quotes());
            return;
        }
        if (isInitial) {
            state.style()->setQuotes(0);
            return;
        }
        if (value->isValueList()) {
            CSSValueList* list = toCSSValueList(value);
            RefPtr<QuotesData> quotes = QuotesData::create();
            for (size_t i = 0; i < list->length(); i += 2) {
                CSSValue* first = list->itemWithoutBoundsCheck(i);
                // item() returns null if out of bounds so this is safe.
                CSSValue* second = list->item(i + 1);
                if (!second)
                    continue;
                String startQuote = toCSSPrimitiveValue(first)->getStringValue();
                String endQuote = toCSSPrimitiveValue(second)->getStringValue();
                quotes->addPair(std::make_pair(startQuote, endQuote));
            }
            state.style()->setQuotes(quotes);
            return;
        }
        if (primitiveValue) {
            if (primitiveValue->getValueID() == CSSValueNone)
                state.style()->setQuotes(QuotesData::create());
        }
        return;
    // Shorthand properties.
    case CSSPropertyFont:
        if (isInherit) {
            FontDescription fontDescription = state.parentStyle()->fontDescription();
            state.style()->setLineHeight(state.parentStyle()->specifiedLineHeight());
            state.setLineHeightValue(0);
            setFontDescription(fontDescription);
        } else if (isInitial) {
            Settings* settings = documentSettings();
            ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
            if (!settings)
                return;
            initializeFontStyle(settings);
        } else if (primitiveValue) {
            state.style()->setLineHeight(RenderStyle::initialLineHeight());
            state.setLineHeightValue(0);

            FontDescription fontDescription;
            RenderTheme::defaultTheme()->systemFont(primitiveValue->getValueID(), fontDescription);

            // Double-check and see if the theme did anything. If not, don't bother updating the font.
            if (fontDescription.isAbsoluteSize()) {
                // Make sure the rendering mode and printer font settings are updated.
                Settings* settings = documentSettings();
                ASSERT(settings); // If we're doing style resolution, this document should always be in a frame and thus have settings
                if (!settings)
                    return;
                fontDescription.setRenderingMode(settings->fontRenderingMode());
                fontDescription.setUsePrinterFont(document()->printing());

                // Handle the zoom factor.
                fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(document(), state.style(), fontDescription.isAbsoluteSize(), fontDescription.specifiedSize(), useSVGZoomRules()));
                setFontDescription(fontDescription);
            }
        } else if (value->isFontValue()) {
            FontValue* font = static_cast<FontValue*>(value);
            if (!font->style || !font->variant || !font->weight
                || !font->size || !font->lineHeight || !font->family)
                return;
            applyProperty(CSSPropertyFontStyle, font->style.get());
            applyProperty(CSSPropertyFontVariant, font->variant.get());
            applyProperty(CSSPropertyFontWeight, font->weight.get());
            // The previous properties can dirty our font but they don't try to read the font's
            // properties back, which is safe. However if font-size is using the 'ex' unit, it will
            // need query the dirtied font's x-height to get the computed size. To be safe in this
            // case, let's just update the font now.
            updateFont();
            applyProperty(CSSPropertyFontSize, font->size.get());

            state.setLineHeightValue(font->lineHeight.get());

            applyProperty(CSSPropertyFontFamily, font->family.get());
        }
        return;

    case CSSPropertyBackground:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBorder:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderColor:
    case CSSPropertyBorderImage:
    case CSSPropertyBorderLeft:
    case CSSPropertyBorderRadius:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderSpacing:
    case CSSPropertyBorderStyle:
    case CSSPropertyBorderTop:
    case CSSPropertyBorderWidth:
    case CSSPropertyListStyle:
    case CSSPropertyMargin:
    case CSSPropertyOutline:
    case CSSPropertyOverflow:
    case CSSPropertyPadding:
    case CSSPropertyTransition:
    case CSSPropertyWebkitAnimation:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderRadius:
    case CSSPropertyWebkitColumns:
    case CSSPropertyWebkitColumnRule:
    case CSSPropertyFlex:
    case CSSPropertyFlexFlow:
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
    case CSSPropertyGridArea:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMarquee:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitMaskRepeat:
    case CSSPropertyWebkitTextEmphasis:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyWebkitTransition:
    case CSSPropertyWebkitTransformOrigin:
        ASSERT(isExpandedShorthand(id));
        ASSERT_NOT_REACHED();
        break;

    // CSS3 Properties
    case CSSPropertyTextShadow:
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow: {
        if (isInherit) {
            if (id == CSSPropertyTextShadow)
                return state.style()->setTextShadow(state.parentStyle()->textShadow() ? adoptPtr(new ShadowData(*state.parentStyle()->textShadow())) : nullptr);
            return state.style()->setBoxShadow(state.parentStyle()->boxShadow() ? adoptPtr(new ShadowData(*state.parentStyle()->boxShadow())) : nullptr);
        }
        if (isInitial || primitiveValue) // initial | none
            return id == CSSPropertyTextShadow ? state.style()->setTextShadow(nullptr) : state.style()->setBoxShadow(nullptr);

        if (!value->isValueList())
            return;

        for (CSSValueListIterator i = value; i.hasMore(); i.advance()) {
            CSSValue* currValue = i.value();
            if (!currValue->isShadowValue())
                continue;
            ShadowValue* item = static_cast<ShadowValue*>(currValue);
            int x = item->x->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor);
            int y = item->y->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor);
            int blur = item->blur ? item->blur->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor) : 0;
            int spread = item->spread ? item->spread->computeLength<int>(state.style(), state.rootElementStyle(), zoomFactor) : 0;
            ShadowStyle shadowStyle = item->style && item->style->getValueID() == CSSValueInset ? Inset : Normal;
            Color color;
            if (item->color)
                color = m_state.colorFromPrimitiveValue(item->color.get());
            else if (state.style())
                color = state.style()->color();

            OwnPtr<ShadowData> shadowData = adoptPtr(new ShadowData(IntPoint(x, y), blur, spread, shadowStyle, color.isValid() ? color : Color::transparent));
            if (id == CSSPropertyTextShadow)
                state.style()->setTextShadow(shadowData.release(), i.index()); // add to the list if this is not the first entry
            else
                state.style()->setBoxShadow(shadowData.release(), i.index()); // add to the list if this is not the first entry
        }
        return;
    }
    case CSSPropertyWebkitBoxReflect: {
        HANDLE_INHERIT_AND_INITIAL(boxReflect, BoxReflect)
        if (primitiveValue) {
            state.style()->setBoxReflect(RenderStyle::initialBoxReflect());
            return;
        }

        if (!value->isReflectValue())
            return;

        CSSReflectValue* reflectValue = static_cast<CSSReflectValue*>(value);
        RefPtr<StyleReflection> reflection = StyleReflection::create();
        reflection->setDirection(*reflectValue->direction());
        if (reflectValue->offset())
            reflection->setOffset(reflectValue->offset()->convertToLength<FixedIntegerConversion | PercentConversion | CalculatedConversion>(state.style(), state.rootElementStyle(), zoomFactor));
        NinePieceImage mask;
        mask.setMaskDefaults();
        m_styleMap.mapNinePieceImage(id, reflectValue->mask(), mask);
        reflection->setMask(mask);

        state.style()->setBoxReflect(reflection.release());
        return;
    }
    case CSSPropertySrc: // Only used in @font-face rules.
        return;
    case CSSPropertyUnicodeRange: // Only used in @font-face rules.
        return;
    case CSSPropertyWebkitLocale: {
        HANDLE_INHERIT_AND_INITIAL(locale, Locale);
        if (!primitiveValue)
            return;
        if (primitiveValue->getValueID() == CSSValueAuto)
            state.style()->setLocale(nullAtom);
        else
            state.style()->setLocale(primitiveValue->getStringValue());
        FontDescription fontDescription = state.style()->fontDescription();
        fontDescription.setScript(localeToScriptCodeForFontSelection(state.style()->locale()));
        setFontDescription(fontDescription);
        return;
    }
    case CSSPropertyWebkitAppRegion: {
        if (!primitiveValue || !primitiveValue->getValueID())
            return;
        state.style()->setDraggableRegionMode(primitiveValue->getValueID() == CSSValueDrag ? DraggableRegionDrag : DraggableRegionNoDrag);
        state.document()->setHasAnnotatedRegions(true);
        return;
    }
    case CSSPropertyWebkitTextStrokeWidth: {
        HANDLE_INHERIT_AND_INITIAL(textStrokeWidth, TextStrokeWidth)
        float width = 0;
        switch (primitiveValue->getValueID()) {
        case CSSValueThin:
        case CSSValueMedium:
        case CSSValueThick: {
            double result = 1.0 / 48;
            if (primitiveValue->getValueID() == CSSValueMedium)
                result *= 3;
            else if (primitiveValue->getValueID() == CSSValueThick)
                result *= 5;
            width = CSSPrimitiveValue::create(result, CSSPrimitiveValue::CSS_EMS)->computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
            break;
        }
        default:
            width = primitiveValue->computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
            break;
        }
        state.style()->setTextStrokeWidth(width);
        return;
    }
    case CSSPropertyWebkitTransform: {
        HANDLE_INHERIT_AND_INITIAL(transform, Transform);
        TransformOperations operations;
        TransformBuilder::createTransformOperations(value, state.style(), state.rootElementStyle(), operations);
        state.style()->setTransform(operations);
        return;
    }
    case CSSPropertyWebkitPerspective: {
        HANDLE_INHERIT_AND_INITIAL(perspective, Perspective)

        if (!primitiveValue)
            return;

        if (primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setPerspective(0);
            return;
        }

        float perspectiveValue;
        if (primitiveValue->isLength())
            perspectiveValue = primitiveValue->computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
        else if (primitiveValue->isNumber()) {
            // For backward compatibility, treat valueless numbers as px.
            perspectiveValue = CSSPrimitiveValue::create(primitiveValue->getDoubleValue(), CSSPrimitiveValue::CSS_PX)->computeLength<float>(state.style(), state.rootElementStyle(), zoomFactor);
        } else
            return;

        if (perspectiveValue >= 0.0f)
            state.style()->setPerspective(perspectiveValue);
        return;
    }
    case CSSPropertyWebkitTapHighlightColor: {
        HANDLE_INHERIT_AND_INITIAL(tapHighlightColor, TapHighlightColor);
        if (!primitiveValue)
            break;

        Color col = m_state.colorFromPrimitiveValue(primitiveValue);
        state.style()->setTapHighlightColor(col);
        return;
    }
    case CSSPropertyInvalid:
        return;
    // Directional properties are resolved by resolveDirectionAwareProperty() before the switch.
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
    {
        CSSPropertyID newId = CSSProperty::resolveDirectionAwareProperty(id, state.style()->direction(), state.style()->writingMode());
        ASSERT(newId != id);
        return applyProperty(newId, value);
    }
    case CSSPropertyFontStretch:
    case CSSPropertyPage:
    case CSSPropertyTextLineThroughColor:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextLineThroughWidth:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextOverlineWidth:
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyTextUnderlineWidth:
    case CSSPropertyWebkitFontSizeDelta:
    case CSSPropertyWebkitTextDecorationsInEffect:
        return;

    // CSS Text Layout Module Level 3: Vertical writing support
    case CSSPropertyWebkitWritingMode: {
        HANDLE_INHERIT_AND_INITIAL(writingMode, WritingMode);

        if (primitiveValue)
            setWritingMode(*primitiveValue);

        // FIXME: It is not ok to modify document state while applying style.
        if (state.element() && state.element() == state.document()->documentElement())
            state.document()->setWritingModeSetOnDocumentElement(true);
        return;
    }

    case CSSPropertyWebkitTextOrientation: {
        HANDLE_INHERIT_AND_INITIAL(textOrientation, TextOrientation);

        if (primitiveValue)
            setTextOrientation(*primitiveValue);

        return;
    }

    case CSSPropertyWebkitLineBoxContain: {
        HANDLE_INHERIT_AND_INITIAL(lineBoxContain, LineBoxContain)
        if (primitiveValue && primitiveValue->getValueID() == CSSValueNone) {
            state.style()->setLineBoxContain(LineBoxContainNone);
            return;
        }

        if (!value->isCSSLineBoxContainValue())
            return;

        CSSLineBoxContainValue* lineBoxContainValue = static_cast<CSSLineBoxContainValue*>(value);
        state.style()->setLineBoxContain(lineBoxContainValue->value());
        return;
    }

    // CSS Fonts Module Level 3
    case CSSPropertyWebkitFontFeatureSettings: {
        if (primitiveValue && primitiveValue->getValueID() == CSSValueNormal) {
            setFontDescription(state.style()->fontDescription().makeNormalFeatureSettings());
            return;
        }

        if (!value->isValueList())
            return;

        FontDescription fontDescription = state.style()->fontDescription();
        CSSValueList* list = toCSSValueList(value);
        RefPtr<FontFeatureSettings> settings = FontFeatureSettings::create();
        int len = list->length();
        for (int i = 0; i < len; ++i) {
            CSSValue* item = list->itemWithoutBoundsCheck(i);
            if (!item->isFontFeatureValue())
                continue;
            FontFeatureValue* feature = static_cast<FontFeatureValue*>(item);
            settings->append(FontFeature(feature->tag(), feature->value()));
        }
        fontDescription.setFeatureSettings(settings.release());
        setFontDescription(fontDescription);
        return;
    }

    case CSSPropertyWebkitFilter: {
        HANDLE_INHERIT_AND_INITIAL(filter, Filter);
        FilterOperations operations;
        if (FilterOperationResolver::createFilterOperations(value, state.style(), state.rootElementStyle(), operations, m_customFilterProgramCache.get(), m_state))
            state.style()->setFilter(operations);
        return;
    }
    case CSSPropertyGridAutoColumns: {
        HANDLE_INHERIT_AND_INITIAL(gridAutoColumns, GridAutoColumns);
        GridTrackSize trackSize;
        if (!createGridTrackSize(value, trackSize, state))
            return;
        state.style()->setGridAutoColumns(trackSize);
        return;
    }
    case CSSPropertyGridAutoRows: {
        HANDLE_INHERIT_AND_INITIAL(gridAutoRows, GridAutoRows);
        GridTrackSize trackSize;
        if (!createGridTrackSize(value, trackSize, state))
            return;
        state.style()->setGridAutoRows(trackSize);
        return;
    }
    case CSSPropertyGridColumns: {
        if (isInherit) {
            m_state.style()->setGridColumns(m_state.parentStyle()->gridColumns());
            m_state.style()->setNamedGridColumnLines(m_state.parentStyle()->namedGridColumnLines());
            return;
        }
        if (isInitial) {
            m_state.style()->setGridColumns(RenderStyle::initialGridColumns());
            m_state.style()->setNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines());
            return;
        }

        Vector<GridTrackSize> trackSizes;
        NamedGridLinesMap namedGridLines;
        if (!createGridTrackList(value, trackSizes, namedGridLines, state))
            return;
        state.style()->setGridColumns(trackSizes);
        state.style()->setNamedGridColumnLines(namedGridLines);
        return;
    }
    case CSSPropertyGridRows: {
        if (isInherit) {
            m_state.style()->setGridRows(m_state.parentStyle()->gridRows());
            m_state.style()->setNamedGridRowLines(m_state.parentStyle()->namedGridRowLines());
            return;
        }
        if (isInitial) {
            m_state.style()->setGridRows(RenderStyle::initialGridRows());
            m_state.style()->setNamedGridRowLines(RenderStyle::initialNamedGridRowLines());
            return;
        }

        Vector<GridTrackSize> trackSizes;
        NamedGridLinesMap namedGridLines;
        if (!createGridTrackList(value, trackSizes, namedGridLines, state))
            return;
        state.style()->setGridRows(trackSizes);
        state.style()->setNamedGridRowLines(namedGridLines);
        return;
    }

    case CSSPropertyGridStart: {
        HANDLE_INHERIT_AND_INITIAL(gridStart, GridStart);
        GridPosition startPosition;
        if (!createGridPosition(value, startPosition))
            return;
        state.style()->setGridStart(startPosition);
        return;
    }
    case CSSPropertyGridEnd: {
        HANDLE_INHERIT_AND_INITIAL(gridEnd, GridEnd);
        GridPosition endPosition;
        if (!createGridPosition(value, endPosition))
            return;
        state.style()->setGridEnd(endPosition);
        return;
    }

    case CSSPropertyGridBefore: {
        HANDLE_INHERIT_AND_INITIAL(gridBefore, GridBefore);
        GridPosition beforePosition;
        if (!createGridPosition(value, beforePosition))
            return;
        state.style()->setGridBefore(beforePosition);
        return;
    }
    case CSSPropertyGridAfter: {
        HANDLE_INHERIT_AND_INITIAL(gridAfter, GridAfter);
        GridPosition afterPosition;
        if (!createGridPosition(value, afterPosition))
            return;
        state.style()->setGridAfter(afterPosition);
        return;
    }

    // These properties are aliased and DeprecatedStyleBuilder already applied the property on the prefixed version.
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionProperty:
    case CSSPropertyTransitionTimingFunction:
        return;
    // These properties are implemented in the DeprecatedStyleBuilder lookup table or in the new StyleBuilder.
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderBottomWidth:
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderImageOutset:
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyBorderImageSlice:
    case CSSPropertyBorderImageSource:
    case CSSPropertyBorderImageWidth:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderRightWidth:
    case CSSPropertyBorderTopColor:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBorderTopWidth:
    case CSSPropertyBottom:
    case CSSPropertyBoxSizing:
    case CSSPropertyCaptionSide:
    case CSSPropertyClear:
    case CSSPropertyClip:
    case CSSPropertyColor:
    case CSSPropertyCounterIncrement:
    case CSSPropertyCounterReset:
    case CSSPropertyCursor:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyEmptyCells:
    case CSSPropertyFloat:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariant:
    case CSSPropertyFontWeight:
    case CSSPropertyHeight:
    case CSSPropertyImageRendering:
    case CSSPropertyLeft:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineHeight:
    case CSSPropertyListStyleImage:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyMaxHeight:
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMixBlendMode:
    case CSSPropertyMinWidth:
    case CSSPropertyOpacity:
    case CSSPropertyOrphans:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyOverflowWrap:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyPointerEvents:
    case CSSPropertyPosition:
    case CSSPropertyResize:
    case CSSPropertyRight:
    case CSSPropertySize:
    case CSSPropertySpeak:
    case CSSPropertyTabSize:
    case CSSPropertyTableLayout:
    case CSSPropertyTextAlign:
    case CSSPropertyTextDecoration:
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyTextIndent:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyTop:
    case CSSPropertyTouchAction:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyVariable:
    case CSSPropertyVerticalAlign:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationFillMode:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationPlayState:
    case CSSPropertyWebkitAnimationTimingFunction:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyWebkitAspectRatio:
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderFit:
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderImage:
    case CSSPropertyWebkitBorderVerticalSpacing:
    case CSSPropertyWebkitBoxAlign:
    case CSSPropertyWebkitBoxDecorationBreak:
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxFlex:
    case CSSPropertyWebkitBoxFlexGroup:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrdinalGroup:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyWebkitColumnAxis:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyWebkitColumnCount:
    case CSSPropertyWebkitColumnGap:
    case CSSPropertyWebkitColumnProgression:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitColumnRuleStyle:
    case CSSPropertyWebkitColumnRuleWidth:
    case CSSPropertyWebkitColumnSpan:
    case CSSPropertyWebkitColumnWidth:
    case CSSPropertyAlignContent:
    case CSSPropertyAlignItems:
    case CSSPropertyAlignSelf:
    case CSSPropertyFlexBasis:
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
    case CSSPropertyFlexWrap:
    case CSSPropertyJustifyContent:
    case CSSPropertyOrder:
    case CSSPropertyWebkitFlowFrom:
    case CSSPropertyWebkitFlowInto:
    case CSSPropertyWebkitFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitFontVariantLigatures:
    case CSSPropertyWebkitHighlight:
    case CSSPropertyWebkitHyphenateCharacter:
    case CSSPropertyWebkitHyphenateLimitAfter:
    case CSSPropertyWebkitHyphenateLimitBefore:
    case CSSPropertyWebkitHyphenateLimitLines:
    case CSSPropertyWebkitHyphens:
    case CSSPropertyWebkitLineAlign:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitLineClamp:
    case CSSPropertyWebkitLineGrid:
    case CSSPropertyWebkitLineSnap:
    case CSSPropertyWebkitMarqueeDirection:
    case CSSPropertyWebkitMarqueeIncrement:
    case CSSPropertyWebkitMarqueeRepetition:
    case CSSPropertyWebkitMarqueeSpeed:
    case CSSPropertyWebkitMarqueeStyle:
    case CSSPropertyWebkitMaskBoxImage:
    case CSSPropertyWebkitMaskBoxImageOutset:
    case CSSPropertyWebkitMaskBoxImageRepeat:
    case CSSPropertyWebkitMaskBoxImageSlice:
    case CSSPropertyWebkitMaskBoxImageSource:
    case CSSPropertyWebkitMaskBoxImageWidth:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitPrintColorAdjust:
    case CSSPropertyWebkitRegionBreakAfter:
    case CSSPropertyWebkitRegionBreakBefore:
    case CSSPropertyWebkitRegionBreakInside:
    case CSSPropertyWebkitRegionFragment:
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
    case CSSPropertyWebkitTextCombine:
#if ENABLE(CSS3_TEXT)
    case CSSPropertyWebkitTextAlignLast:
    case CSSPropertyWebkitTextUnderlinePosition:
#endif // CSS3_TEXT
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextEmphasisPosition:
    case CSSPropertyWebkitTextEmphasisStyle:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitTransitionDelay:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionProperty:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWebkitClipPath:
    case CSSPropertyWebkitWrapFlow:
    case CSSPropertyWebkitShapeMargin:
    case CSSPropertyWebkitShapePadding:
    case CSSPropertyWebkitWrapThrough:
    case CSSPropertyWebkitShapeInside:
    case CSSPropertyWebkitShapeOutside:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWidows:
    case CSSPropertyWidth:
    case CSSPropertyWordBreak:
    case CSSPropertyWordSpacing:
    case CSSPropertyWordWrap:
    case CSSPropertyZIndex:
    case CSSPropertyZoom:
    case CSSPropertyMaxZoom:
    case CSSPropertyMinZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
        ASSERT_NOT_REACHED();
        return;
    default:
        // Try the SVG properties
        applySVGProperty(id, value);
        return;
    }
}

PassRefPtr<StyleImage> StyleResolver::styleImage(CSSPropertyID property, CSSValue* value)
{
    if (value->isImageValue())
        return cachedOrPendingFromValue(property, toCSSImageValue(value));

    if (value->isImageGeneratorValue()) {
        if (value->isGradientValue())
            return generatedOrPendingFromValue(property, static_cast<CSSGradientValue*>(value)->gradientWithStylesResolved(this).get());
        return generatedOrPendingFromValue(property, static_cast<CSSImageGeneratorValue*>(value));
    }

    if (value->isImageSetValue())
        return setOrPendingFromValue(property, static_cast<CSSImageSetValue*>(value));

    if (value->isCursorImageValue())
        return cursorOrPendingFromValue(property, static_cast<CSSCursorImageValue*>(value));

    return 0;
}

PassRefPtr<StyleImage> StyleResolver::cachedOrPendingFromValue(CSSPropertyID property, CSSImageValue* value)
{
    RefPtr<StyleImage> image = value->cachedOrPendingImage();
    if (image && image->isPendingImage())
        m_state.pendingImageProperties().set(property, value);
    return image.release();
}

PassRefPtr<StyleImage> StyleResolver::generatedOrPendingFromValue(CSSPropertyID property, CSSImageGeneratorValue* value)
{
    if (value->isPending()) {
        m_state.pendingImageProperties().set(property, value);
        return StylePendingImage::create(value);
    }
    return StyleGeneratedImage::create(value);
}

PassRefPtr<StyleImage> StyleResolver::setOrPendingFromValue(CSSPropertyID property, CSSImageSetValue* value)
{
    RefPtr<StyleImage> image = value->cachedOrPendingImageSet(document());
    if (image && image->isPendingImage())
        m_state.pendingImageProperties().set(property, value);
    return image.release();
}

PassRefPtr<StyleImage> StyleResolver::cursorOrPendingFromValue(CSSPropertyID property, CSSCursorImageValue* value)
{
    RefPtr<StyleImage> image = value->cachedOrPendingImage(document());
    if (image && image->isPendingImage())
        m_state.pendingImageProperties().set(property, value);
    return image.release();
}

void StyleResolver::checkForZoomChange(RenderStyle* style, RenderStyle* parentStyle)
{
    if (style->effectiveZoom() == parentStyle->effectiveZoom())
        return;

    const FontDescription& childFont = style->fontDescription();
    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, childFont.specifiedSize());
    style->setFontDescription(newFontDescription);
}

void StyleResolver::checkForGenericFamilyChange(RenderStyle* style, RenderStyle* parentStyle)
{
    const FontDescription& childFont = style->fontDescription();

    if (childFont.isAbsoluteSize() || !parentStyle)
        return;

    const FontDescription& parentFont = parentStyle->fontDescription();
    if (childFont.useFixedDefaultSize() == parentFont.useFixedDefaultSize())
        return;

    // For now, lump all families but monospace together.
    if (childFont.genericFamily() != FontDescription::MonospaceFamily
        && parentFont.genericFamily() != FontDescription::MonospaceFamily)
        return;

    // We know the parent is monospace or the child is monospace, and that font
    // size was unspecified. We want to scale our font size as appropriate.
    // If the font uses a keyword size, then we refetch from the table rather than
    // multiplying by our scale factor.
    float size;
    if (childFont.keywordSize())
        size = FontSize::fontSizeForKeyword(document(), CSSValueXxSmall + childFont.keywordSize() - 1, childFont.useFixedDefaultSize());
    else {
        Settings* settings = documentSettings();
        float fixedScaleFactor = (settings && settings->defaultFixedFontSize() && settings->defaultFontSize())
            ? static_cast<float>(settings->defaultFixedFontSize()) / settings->defaultFontSize()
            : 1;
        size = parentFont.useFixedDefaultSize() ?
            childFont.specifiedSize() / fixedScaleFactor :
            childFont.specifiedSize() * fixedScaleFactor;
    }

    FontDescription newFontDescription(childFont);
    setFontSize(newFontDescription, size);
    style->setFontDescription(newFontDescription);
}

void StyleResolver::initializeFontStyle(Settings* settings)
{
    FontDescription fontDescription;
    fontDescription.setGenericFamily(FontDescription::StandardFamily);
    fontDescription.setRenderingMode(settings->fontRenderingMode());
    fontDescription.setUsePrinterFont(document()->printing());
    const AtomicString& standardFontFamily = documentSettings()->standardFontFamily();
    if (!standardFontFamily.isEmpty()) {
        fontDescription.firstFamily().setFamily(standardFontFamily);
        fontDescription.firstFamily().appendFamily(0);
    }
    fontDescription.setKeywordSize(CSSValueMedium - CSSValueXxSmall + 1);
    setFontSize(fontDescription, FontSize::fontSizeForKeyword(document(), CSSValueMedium, false));
    m_state.style()->setLineHeight(RenderStyle::initialLineHeight());
    m_state.setLineHeightValue(0);
    setFontDescription(fontDescription);
}

void StyleResolver::setFontSize(FontDescription& fontDescription, float size)
{
    fontDescription.setSpecifiedSize(size);
    fontDescription.setComputedSize(getComputedSizeFromSpecifiedSize(document(), m_state.style(), fontDescription.isAbsoluteSize(), size, useSVGZoomRules()));
}

bool StyleResolver::colorFromPrimitiveValueIsDerivedFromElement(CSSPrimitiveValue* value)
{
    int ident = value->getValueID();
    switch (ident) {
    case CSSValueWebkitText:
    case CSSValueWebkitLink:
    case CSSValueWebkitActivelink:
    case CSSValueCurrentcolor:
        return true;
    default:
        return false;
    }
}

void StyleResolver::addViewportDependentMediaQueryResult(const MediaQueryExp* expr, bool result)
{
    m_viewportDependentMediaQueryResults.append(adoptPtr(new MediaQueryResult(*expr, result)));
}

bool StyleResolver::affectedByViewportChange() const
{
    unsigned s = m_viewportDependentMediaQueryResults.size();
    for (unsigned i = 0; i < s; i++) {
        if (m_medium->eval(&m_viewportDependentMediaQueryResults[i]->m_expression) != m_viewportDependentMediaQueryResults[i]->m_result)
            return true;
    }
    return false;
}

void StyleResolver::loadPendingSVGDocuments()
{
    StyleResolverState& state = m_state;
    if (!state.style()->hasFilter() || state.pendingSVGDocuments().isEmpty())
        return;

    CachedResourceLoader* cachedResourceLoader = state.document()->cachedResourceLoader();
    Vector<RefPtr<FilterOperation> >& filterOperations = state.style()->mutableFilter().operations();
    for (unsigned i = 0; i < filterOperations.size(); ++i) {
        RefPtr<FilterOperation> filterOperation = filterOperations.at(i);
        if (filterOperation->getOperationType() == FilterOperation::REFERENCE) {
            ReferenceFilterOperation* referenceFilter = static_cast<ReferenceFilterOperation*>(filterOperation.get());

            CSSSVGDocumentValue* value = state.pendingSVGDocuments().get(referenceFilter);
            if (!value)
                continue;
            CachedDocument* cachedDocument = value->load(cachedResourceLoader);
            if (!cachedDocument)
                continue;

            // Stash the CachedDocument on the reference filter.
            referenceFilter->setCachedSVGDocumentReference(adoptPtr(new CachedSVGDocumentReference(cachedDocument)));
        }
    }
    state.pendingSVGDocuments().clear();
}

void StyleResolver::loadPendingShaders()
{
    if (!m_state.style()->hasFilter() || !m_state.hasPendingShaders())
        return;

    CachedResourceLoader* cachedResourceLoader = m_state.document()->cachedResourceLoader();

    Vector<RefPtr<FilterOperation> >& filterOperations = m_state.style()->mutableFilter().operations();
    for (unsigned i = 0; i < filterOperations.size(); ++i) {
        RefPtr<FilterOperation> filterOperation = filterOperations.at(i);
        if (filterOperation->getOperationType() == FilterOperation::CUSTOM) {
            CustomFilterOperation* customFilter = static_cast<CustomFilterOperation*>(filterOperation.get());
            ASSERT(customFilter->program());
            StyleCustomFilterProgram* program = static_cast<StyleCustomFilterProgram*>(customFilter->program());
            // Note that the StylePendingShaders could be already resolved to StyleCachedShaders. That's because the rule was matched before.
            // However, the StyleCustomFilterProgram that was initially created could have been removed from the cache in the meanwhile,
            // meaning that we get a new StyleCustomFilterProgram here that is not yet in the cache, but already has loaded StyleShaders.
            if (!program->hasPendingShaders() && program->inCache())
                continue;
            if (!m_customFilterProgramCache)
                m_customFilterProgramCache = adoptPtr(new StyleCustomFilterProgramCache());
            RefPtr<StyleCustomFilterProgram> styleProgram = m_customFilterProgramCache->lookup(program);
            if (styleProgram.get())
                customFilter->setProgram(styleProgram.release());
            else {
                if (program->vertexShader() && program->vertexShader()->isPendingShader()) {
                    CSSShaderValue* shaderValue = static_cast<StylePendingShader*>(program->vertexShader())->cssShaderValue();
                    program->setVertexShader(shaderValue->cachedShader(cachedResourceLoader));
                }
                if (program->fragmentShader() && program->fragmentShader()->isPendingShader()) {
                    CSSShaderValue* shaderValue = static_cast<StylePendingShader*>(program->fragmentShader())->cssShaderValue();
                    program->setFragmentShader(shaderValue->cachedShader(cachedResourceLoader));
                }
                m_customFilterProgramCache->add(program);
            }
        }
    }
    m_state.setHasPendingShaders(false);
}

PassRefPtr<StyleImage> StyleResolver::loadPendingImage(StylePendingImage* pendingImage)
{
    CachedResourceLoader* cachedResourceLoader = m_state.document()->cachedResourceLoader();

    if (pendingImage->cssImageValue()) {
        CSSImageValue* imageValue = pendingImage->cssImageValue();
        return imageValue->cachedImage(cachedResourceLoader);
    }

    if (pendingImage->cssImageGeneratorValue()) {
        CSSImageGeneratorValue* imageGeneratorValue = pendingImage->cssImageGeneratorValue();
        imageGeneratorValue->loadSubimages(cachedResourceLoader);
        return StyleGeneratedImage::create(imageGeneratorValue);
    }

    if (pendingImage->cssCursorImageValue()) {
        CSSCursorImageValue* cursorImageValue = pendingImage->cssCursorImageValue();
        return cursorImageValue->cachedImage(cachedResourceLoader);
    }

    if (pendingImage->cssImageSetValue()) {
        CSSImageSetValue* imageSetValue = pendingImage->cssImageSetValue();
        return imageSetValue->cachedImageSet(cachedResourceLoader);
    }

    return 0;
}

void StyleResolver::loadPendingShapeImage(ShapeValue* shapeValue)
{
    DEFINE_STATIC_LOCAL(ResourceLoaderOptions, options, (SendCallbacks, SniffContent, BufferData, AllowStoredCredentials, ClientRequestedCredentials, AskClientForCrossOriginCredentials, DoSecurityCheck, CheckContentSecurityPolicy, RestrictToSameOrigin));

    StyleImage* image = shapeValue->image();
    if (!image)
        return;

    StylePendingImage* pendingImage = static_cast<StylePendingImage*>(image);
    CSSImageValue* cssImageValue =  pendingImage->cssImageValue();
    CachedResourceLoader* cachedResourceLoader = m_state.document()->cachedResourceLoader();

    shapeValue->setImage(cssImageValue->cachedImage(cachedResourceLoader, options));
}

void StyleResolver::loadPendingImages()
{
    if (m_state.pendingImageProperties().isEmpty())
        return;

    PendingImagePropertyMap::const_iterator::Keys end = m_state.pendingImageProperties().end().keys();
    for (PendingImagePropertyMap::const_iterator::Keys it = m_state.pendingImageProperties().begin().keys(); it != end; ++it) {
        CSSPropertyID currentProperty = *it;

        switch (currentProperty) {
        case CSSPropertyBackgroundImage: {
            for (FillLayer* backgroundLayer = m_state.style()->accessBackgroundLayers(); backgroundLayer; backgroundLayer = backgroundLayer->next()) {
                if (backgroundLayer->image() && backgroundLayer->image()->isPendingImage())
                    backgroundLayer->setImage(loadPendingImage(static_cast<StylePendingImage*>(backgroundLayer->image())));
            }
            break;
        }
        case CSSPropertyContent: {
            for (ContentData* contentData = const_cast<ContentData*>(m_state.style()->contentData()); contentData; contentData = contentData->next()) {
                if (contentData->isImage()) {
                    StyleImage* image = static_cast<ImageContentData*>(contentData)->image();
                    if (image->isPendingImage()) {
                        RefPtr<StyleImage> loadedImage = loadPendingImage(static_cast<StylePendingImage*>(image));
                        if (loadedImage)
                            static_cast<ImageContentData*>(contentData)->setImage(loadedImage.release());
                    }
                }
            }
            break;
        }
        case CSSPropertyCursor: {
            if (CursorList* cursorList = m_state.style()->cursors()) {
                for (size_t i = 0; i < cursorList->size(); ++i) {
                    CursorData& currentCursor = cursorList->at(i);
                    if (StyleImage* image = currentCursor.image()) {
                        if (image->isPendingImage())
                            currentCursor.setImage(loadPendingImage(static_cast<StylePendingImage*>(image)));
                    }
                }
            }
            break;
        }
        case CSSPropertyListStyleImage: {
            if (m_state.style()->listStyleImage() && m_state.style()->listStyleImage()->isPendingImage())
                m_state.style()->setListStyleImage(loadPendingImage(static_cast<StylePendingImage*>(m_state.style()->listStyleImage())));
            break;
        }
        case CSSPropertyBorderImageSource: {
            if (m_state.style()->borderImageSource() && m_state.style()->borderImageSource()->isPendingImage())
                m_state.style()->setBorderImageSource(loadPendingImage(static_cast<StylePendingImage*>(m_state.style()->borderImageSource())));
            break;
        }
        case CSSPropertyWebkitBoxReflect: {
            if (StyleReflection* reflection = m_state.style()->boxReflect()) {
                const NinePieceImage& maskImage = reflection->mask();
                if (maskImage.image() && maskImage.image()->isPendingImage()) {
                    RefPtr<StyleImage> loadedImage = loadPendingImage(static_cast<StylePendingImage*>(maskImage.image()));
                    reflection->setMask(NinePieceImage(loadedImage.release(), maskImage.imageSlices(), maskImage.fill(), maskImage.borderSlices(), maskImage.outset(), maskImage.horizontalRule(), maskImage.verticalRule()));
                }
            }
            break;
        }
        case CSSPropertyWebkitMaskBoxImageSource: {
            if (m_state.style()->maskBoxImageSource() && m_state.style()->maskBoxImageSource()->isPendingImage())
                m_state.style()->setMaskBoxImageSource(loadPendingImage(static_cast<StylePendingImage*>(m_state.style()->maskBoxImageSource())));
            break;
        }
        case CSSPropertyWebkitMaskImage: {
            for (FillLayer* maskLayer = m_state.style()->accessMaskLayers(); maskLayer; maskLayer = maskLayer->next()) {
                if (maskLayer->image() && maskLayer->image()->isPendingImage())
                    maskLayer->setImage(loadPendingImage(static_cast<StylePendingImage*>(maskLayer->image())));
            }
            break;
        }
        case CSSPropertyWebkitShapeInside:
            loadPendingShapeImage(m_state.style()->shapeInside());
            break;
        case CSSPropertyWebkitShapeOutside:
            loadPendingShapeImage(m_state.style()->shapeOutside());
            break;
        default:
            ASSERT_NOT_REACHED();
        }
    }

    m_state.pendingImageProperties().clear();
}

void StyleResolver::loadPendingResources()
{
    // Start loading images referenced by this style.
    loadPendingImages();

    // Start loading the shaders referenced by this style.
    loadPendingShaders();

    // Start loading the SVG Documents referenced by this style.
    loadPendingSVGDocuments();
}

inline StyleResolver::MatchedProperties::MatchedProperties()
    : possiblyPaddedMember(0)
{
}

inline StyleResolver::MatchedProperties::~MatchedProperties()
{
}

void StyleResolver::MatchedProperties::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(properties, "properties");
}

void StyleResolver::MatchedPropertiesCacheItem::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(matchedProperties, "matchedProperties");
    info.addMember(ranges, "ranges");
    info.addMember(renderStyle, "renderStyle");
    info.addMember(parentRenderStyle, "parentRenderStyle");
}

void MediaQueryResult::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_expression, "expression");
}

void StyleResolver::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::CSS);
    info.addMember(m_ruleSets, "ruleSets");
    info.addMember(m_matchedPropertiesCache, "matchedPropertiesCache");
    info.addMember(m_matchedPropertiesCacheSweepTimer, "matchedPropertiesCacheSweepTimer");

    info.addMember(m_medium, "medium");
    info.addMember(m_rootDefaultStyle, "rootDefaultStyle");
    info.addMember(m_document, "document");

    info.addMember(m_fontSelector, "fontSelector");
    info.addMember(m_viewportDependentMediaQueryResults, "viewportDependentMediaQueryResults");
    info.ignoreMember(m_styleBuilder);
    info.addMember(m_inspectorCSSOMWrappers);

    info.addMember(m_styleTree, "scopedStyleTree");
    info.addMember(m_state, "state");

    // FIXME: move this to a place where it would be called only once?
    info.addMember(CSSDefaultStyleSheets::defaultStyle, "defaultStyle");
    info.addMember(CSSDefaultStyleSheets::defaultQuirksStyle, "defaultQuirksStyle");
    info.addMember(CSSDefaultStyleSheets::defaultPrintStyle, "defaultPrintStyle");
    info.addMember(CSSDefaultStyleSheets::defaultViewSourceStyle, "defaultViewSourceStyle");
}

} // namespace WebCore
