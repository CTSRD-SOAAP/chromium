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
#include "StylePropertyShorthand.h"
#include "core/animation/ActiveAnimations.h"
#include "core/animation/AnimatableLength.h"
#include "core/animation/AnimatableValue.h"
#include "core/animation/Animation.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/css/CSSAnimatableValueFactory.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSDefaultStyleSheets.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/ElementRuleCollector.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/PageRuleCollector.h"
#include "core/css/RuleSet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/AnimatedStyleBuilder.h"
#include "core/css/resolver/MatchResult.h"
#include "core/css/resolver/MediaQueryResult.h"
#include "core/css/resolver/SharedStyleFinder.h"
#include "core/css/resolver/StyleAdjuster.h"
#include "core/css/resolver/StyleBuilder.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameView.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/KeyframeList.h"
#include "core/rendering/style/StyleCustomFilterProgramCache.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGFontFaceElement.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"

using namespace std;

namespace {

using namespace WebCore;

PassRefPtr<TimingFunction> generateTimingFunction(const KeyframeAnimationEffect::KeyframeVector keyframes, const HashMap<double, RefPtr<TimingFunction> > perKeyframeTimingFunctions)
{
    // Generate the chained timing function. Note that timing functions apply
    // from the keyframe in which they're specified to the next keyframe.
    bool isTimingFunctionLinearThroughout = true;
    RefPtr<ChainedTimingFunction> chainedTimingFunction = ChainedTimingFunction::create();
    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        double lowerBound = keyframes[i]->offset();
        ASSERT(lowerBound >=0 && lowerBound < 1);
        double upperBound = keyframes[i + 1]->offset();
        ASSERT(upperBound > 0 && upperBound <= 1);
        TimingFunction* timingFunction = perKeyframeTimingFunctions.get(lowerBound);
        isTimingFunctionLinearThroughout &= timingFunction->type() == TimingFunction::LinearFunction;
        chainedTimingFunction->appendSegment(upperBound, timingFunction);
    }
    if (isTimingFunctionLinearThroughout)
        return LinearTimingFunction::create();
    return chainedTimingFunction;
}

} // namespace

namespace WebCore {

using namespace HTMLNames;

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

StyleResolver::StyleResolver(Document& document, bool matchAuthorAndUserStyles)
    : m_document(document)
    , m_matchAuthorAndUserStyles(matchAuthorAndUserStyles)
    , m_fontSelector(CSSFontSelector::create(&document))
    , m_viewportStyleResolver(ViewportStyleResolver::create(&document))
    , m_styleResourceLoader(document.fetcher())
{
    Element* root = document.documentElement();

    m_fontSelector->registerForInvalidationCallbacks(this);

    CSSDefaultStyleSheets::initDefaultStyle(root);

    // construct document root element default style. this is needed
    // to evaluate media queries that contain relative constraints, like "screen and (max-width: 10em)"
    // This is here instead of constructor, because when constructor is run,
    // document doesn't have documentElement
    // NOTE: this assumes that element that gets passed to styleForElement -call
    // is always from the document that owns the style selector
    FrameView* view = document.view();
    if (view)
        m_medium = adoptPtr(new MediaQueryEvaluator(view->mediaType()));
    else
        m_medium = adoptPtr(new MediaQueryEvaluator("all"));

    if (root)
        m_rootDefaultStyle = styleForElement(root, 0, DisallowStyleSharing, MatchOnlyUserAgentRules);

    if (m_rootDefaultStyle && view)
        m_medium = adoptPtr(new MediaQueryEvaluator(view->mediaType(), &view->frame(), m_rootDefaultStyle.get()));

    m_styleTree.clear();

    StyleEngine* styleSheetCollection = document.styleEngine();
    m_ruleSets.initUserStyle(styleSheetCollection, CSSSelectorWatch::from(document).watchedCallbackSelectors(), *m_medium, *this);

#if ENABLE(SVG_FONTS)
    if (document.svgExtensions()) {
        const HashSet<SVGFontFaceElement*>& svgFontFaceElements = document.svgExtensions()->svgFontFaceElements();
        HashSet<SVGFontFaceElement*>::const_iterator end = svgFontFaceElements.end();
        for (HashSet<SVGFontFaceElement*>::const_iterator it = svgFontFaceElements.begin(); it != end; ++it)
            fontSelector()->addFontFaceRule((*it)->fontFaceRule());
    }
#endif

    styleSheetCollection->appendActiveAuthorStyleSheets(this);
}

void StyleResolver::appendAuthorStyleSheets(unsigned firstNew, const Vector<RefPtr<CSSStyleSheet> >& styleSheets)
{
    // This handles sheets added to the end of the stylesheet list only. In other cases the style resolver
    // needs to be reconstructed. To handle insertions too the rule order numbers would need to be updated.
    unsigned size = styleSheets.size();
    for (unsigned i = firstNew; i < size; ++i) {
        CSSStyleSheet* cssSheet = styleSheets[i].get();
        ASSERT(!cssSheet->disabled());
        if (cssSheet->mediaQueries() && !m_medium->eval(cssSheet->mediaQueries(), &m_viewportDependentMediaQueryResults))
            continue;

        StyleSheetContents* sheet = cssSheet->contents();
        const ContainerNode* scopingNode = ScopedStyleResolver::scopingNodeFor(cssSheet);
        if (!scopingNode && cssSheet->ownerNode() && cssSheet->ownerNode()->isInShadowTree())
            continue;

        ScopedStyleResolver* resolver = ensureScopedStyleResolver(scopingNode);
        ASSERT(resolver);
        resolver->addRulesFromSheet(sheet, *m_medium, this);
        m_inspectorCSSOMWrappers.collectFromStyleSheetIfNeeded(cssSheet);
    }
}

void StyleResolver::finishAppendAuthorStyleSheets()
{
    collectFeatures();

    if (document().renderer() && document().renderer()->style())
        document().renderer()->style()->font().update(fontSelector());

    collectViewportRules();
}

void StyleResolver::resetAuthorStyle(const ContainerNode* scopingNode)
{
    // FIXME: When chanking scoped attribute, scopingNode's hasScopedHTMLStyleChild has been already modified.
    // So we cannot use hasScopedHTMLStyleChild flag here.
    ScopedStyleResolver* resolver = scopingNode ? m_styleTree.lookupScopedStyleResolverFor(scopingNode) : m_styleTree.scopedStyleResolverForDocument();
    if (!resolver)
        return;

    m_ruleSets.treeBoundaryCrossingRules().reset(scopingNode);

    resolver->resetAuthorStyle();
    if (!scopingNode)
        return;

    if (scopingNode->isInShadowTree())
        resetAtHostRules(scopingNode->containingShadowRoot());

    if (!resolver->hasOnlyEmptyRuleSets())
        return;

    m_styleTree.remove(scopingNode);
}

void StyleResolver::resetAtHostRules(const ShadowRoot* shadowRoot)
{
    if (!shadowRoot)
        return;

    const ContainerNode* shadowHost = shadowRoot->shadowHost();
    ASSERT(shadowHost);
    ScopedStyleResolver* resolver = m_styleTree.lookupScopedStyleResolverFor(shadowHost);
    if (!resolver)
        return;

    resolver->resetAtHostRules(shadowRoot);
    if (!resolver->hasOnlyEmptyRuleSets())
        return;

    m_styleTree.remove(shadowHost);
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
    m_ruleSets.collectFeaturesTo(m_features, document().isViewSource());
    m_styleTree.collectFeaturesTo(m_features);

    m_siblingRuleSet = makeRuleSet(m_features.siblingRules);
    m_uncommonAttributeRuleSet = makeRuleSet(m_features.uncommonAttributeRules);
}

bool StyleResolver::hasRulesForId(const AtomicString& id) const
{
    return m_features.idsInRules.contains(id.impl());
}

void StyleResolver::addToStyleSharingList(Element* element)
{
    if (m_styleSharingList.size() >= styleSharingListSize)
        m_styleSharingList.remove(--m_styleSharingList.end());
    m_styleSharingList.prepend(element);
}

void StyleResolver::clearStyleSharingList()
{
    m_styleSharingList.clear();
}

void StyleResolver::fontsNeedUpdate(FontSelector* fontSelector)
{
    invalidateMatchedPropertiesCache();
    m_document.setNeedsStyleRecalc();
}

void StyleResolver::pushParentElement(Element* parent)
{
    ASSERT(parent);
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
    m_styleTree.pushStyleCache(*parent, parent->parentOrShadowHostNode());
}

void StyleResolver::popParentElement(Element* parent)
{
    ASSERT(parent);
    // Note that we may get invoked for some random elements in some wacky cases during style resolve.
    // Pause maintaining the stack in this case.
    if (m_selectorFilter.parentStackIsConsistent(parent))
        m_selectorFilter.popParent();

    m_styleTree.popStyleCache(*parent);
}

void StyleResolver::pushParentShadowRoot(const ShadowRoot& shadowRoot)
{
    ASSERT(shadowRoot.host());
    m_styleTree.pushStyleCache(shadowRoot, shadowRoot.host());
}

void StyleResolver::popParentShadowRoot(const ShadowRoot& shadowRoot)
{
    ASSERT(shadowRoot.host());
    m_styleTree.popStyleCache(shadowRoot);
}

StyleResolver::~StyleResolver()
{
    m_fontSelector->unregisterForInvalidationCallbacks(this);
    m_fontSelector->clearDocument();
    m_viewportStyleResolver->clearDocument();
}

inline void StyleResolver::collectTreeBoundaryCrossingRules(ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (m_ruleSets.treeBoundaryCrossingRules().isEmpty())
        return;

    bool previousCanUseFastReject = collector.canUseFastReject();
    collector.setCanUseFastReject(false);

    RuleRange ruleRange = collector.matchedResult().ranges.authorRuleRange();

    TreeBoundaryCrossingRules& rules = m_ruleSets.treeBoundaryCrossingRules();
    CascadeOrder cascadeOrder = 0;

    DocumentOrderedList::iterator it = rules.end();
    while (it != rules.begin()) {
        --it;
        const ContainerNode* scopingNode = toContainerNode(*it);
        RuleSet* ruleSet = rules.ruleSetScopedBy(scopingNode);
        unsigned boundaryBehavior = SelectorChecker::CrossesBoundary | SelectorChecker::ScopeContainsLastMatchedElement;

        if (scopingNode && scopingNode->isShadowRoot()) {
            boundaryBehavior |= SelectorChecker::ScopeIsShadowHost;
            scopingNode = toShadowRoot(scopingNode)->host();
        }
        collector.collectMatchingRules(MatchRequest(ruleSet, includeEmptyRules, scopingNode), ruleRange, static_cast<SelectorChecker::BehaviorAtBoundary>(boundaryBehavior), ignoreCascadeScope, cascadeOrder++);
    }
    collector.setCanUseFastReject(previousCanUseFastReject);
}

void StyleResolver::matchHostRules(Element* element, ScopedStyleResolver* resolver, ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (element != &resolver->scopingNode())
        return;
    resolver->matchHostRules(collector, includeEmptyRules);
}

static inline bool applyAuthorStylesOf(const Element* element)
{
    return element->treeScope().applyAuthorStyles() || (element->shadow() && element->shadow()->applyAuthorStyles());
}

void StyleResolver::matchAuthorRulesForShadowHost(Element* element, ElementRuleCollector& collector, bool includeEmptyRules, Vector<ScopedStyleResolver*, 8>& resolvers, Vector<ScopedStyleResolver*, 8>& resolversInShadowTree)
{
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;

    CascadeScope cascadeScope = 0;
    CascadeOrder cascadeOrder = 0;
    bool applyAuthorStyles = applyAuthorStylesOf(element);

    for (int j = resolversInShadowTree.size() - 1; j >= 0; --j)
        resolversInShadowTree.at(j)->collectMatchingAuthorRules(collector, includeEmptyRules, applyAuthorStyles, cascadeScope, cascadeOrder++);

    if (resolvers.isEmpty() || resolvers.first()->treeScope() != element->treeScope())
        ++cascadeScope;
    cascadeOrder += resolvers.size();
    for (unsigned i = 0; i < resolvers.size(); ++i)
        resolvers.at(i)->collectMatchingAuthorRules(collector, includeEmptyRules, applyAuthorStyles, cascadeScope++, --cascadeOrder);

    collectTreeBoundaryCrossingRules(collector, includeEmptyRules);
    collector.sortAndTransferMatchedRules();

    if (!resolvers.isEmpty())
        matchHostRules(element, resolvers.first(), collector, includeEmptyRules);
}

void StyleResolver::matchAuthorRules(Element* element, ElementRuleCollector& collector, bool includeEmptyRules)
{
    if (m_styleTree.hasOnlyScopedResolverForDocument()) {
        m_styleTree.scopedStyleResolverForDocument()->matchAuthorRules(collector, includeEmptyRules, applyAuthorStylesOf(element));

        collector.clearMatchedRules();
        collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;
        collectTreeBoundaryCrossingRules(collector, includeEmptyRules);
        collector.sortAndTransferMatchedRules();
        return;
    }

    Vector<ScopedStyleResolver*, 8> resolvers;
    m_styleTree.resolveScopedStyles(element, resolvers);

    Vector<ScopedStyleResolver*, 8> resolversInShadowTree;
    m_styleTree.collectScopedResolversForHostedShadowTrees(element, resolversInShadowTree);
    if (!resolversInShadowTree.isEmpty()) {
        matchAuthorRulesForShadowHost(element, collector, includeEmptyRules, resolvers, resolversInShadowTree);
        return;
    }

    if (resolvers.isEmpty())
        return;

    bool applyAuthorStyles = applyAuthorStylesOf(element);
    CascadeScope cascadeScope = 0;
    CascadeOrder cascadeOrder = resolvers.size();
    collector.clearMatchedRules();
    collector.matchedResult().ranges.lastAuthorRule = collector.matchedResult().matchedProperties.size() - 1;

    for (unsigned i = 0; i < resolvers.size(); ++i, --cascadeOrder) {
        ScopedStyleResolver* resolver = resolvers.at(i);
        // FIXME: Need to clarify how to treat style scoped.
        resolver->collectMatchingAuthorRules(collector, includeEmptyRules, applyAuthorStyles, cascadeScope++, resolver->treeScope() == element->treeScope() && resolver->scopingNode().isShadowRoot() ? 0 : cascadeOrder);
    }

    collectTreeBoundaryCrossingRules(collector, includeEmptyRules);
    collector.sortAndTransferMatchedRules();

    matchHostRules(element, resolvers.first(), collector, includeEmptyRules);
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
    if (document().inQuirksMode())
        matchUARules(collector, CSSDefaultStyleSheets::defaultQuirksStyle);

    // If document uses view source styles (in view source mode or in xml viewer mode), then we match rules from the view source style sheet.
    if (document().isViewSource())
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

void StyleResolver::matchAllRules(StyleResolverState& state, ElementRuleCollector& collector, bool matchAuthorAndUserStyles, bool includeSMILProperties)
{
    matchUARules(collector);

    // Now we check user sheet rules.
    if (matchAuthorAndUserStyles)
        matchUserRules(collector, false);

    // Now check author rules, beginning first with presentational attributes mapped from HTML.
    if (state.element()->isStyledElement()) {
        collector.addElementStyleProperties(state.element()->presentationAttributeStyle());

        // Now we check additional mapped declarations.
        // Tables and table cells share an additional mapped rule that must be applied
        // after all attributes, since their mapped style depends on the values of multiple attributes.
        collector.addElementStyleProperties(state.element()->additionalPresentationAttributeStyle());

        if (state.element()->isHTMLElement()) {
            bool isAuto;
            TextDirection textDirection = toHTMLElement(state.element())->directionalityIfhasDirAutoAttribute(isAuto);
            if (isAuto)
                collector.matchedResult().addMatchedProperties(textDirection == LTR ? leftToRightDeclaration() : rightToLeftDeclaration());
        }
    }

    // Check the rules in author sheets next.
    if (matchAuthorAndUserStyles)
        matchAuthorRules(state.element(), collector, false);

    if (state.element()->isStyledElement()) {
        // Now check our inline style attribute.
        if (matchAuthorAndUserStyles && state.element()->inlineStyle()) {
            // Inline style is immutable as long as there is no CSSOM wrapper.
            // FIXME: Media control shadow trees seem to have problems with caching.
            bool isInlineStyleCacheable = !state.element()->inlineStyle()->isMutable() && !state.element()->isInShadowTree();
            // FIXME: Constify.
            collector.addElementStyleProperties(state.element()->inlineStyle(), isInlineStyleCacheable);
        }

        // Now check SMIL animation override style.
        if (includeSMILProperties && matchAuthorAndUserStyles && state.element()->isSVGElement())
            collector.addElementStyleProperties(toSVGElement(state.element())->animatedSMILStyleProperties(), false /* isCacheable */);

        if (state.element()->hasActiveAnimations())
            collector.matchedResult().isCacheable = false;
    }
}

PassRefPtr<RenderStyle> StyleResolver::styleForDocument(Document& document, CSSFontSelector* fontSelector)
{
    const Frame* frame = document.frame();

    // HTML5 states that seamless iframes should replace default CSS values
    // with values inherited from the containing iframe element. However,
    // some values (such as the case of designMode = "on") still need to
    // be set by this "document style".
    RefPtr<RenderStyle> documentStyle = RenderStyle::create();
    bool seamlessWithParent = document.shouldDisplaySeamlesslyWithParent();
    if (seamlessWithParent) {
        RenderStyle* iframeStyle = document.seamlessParentIFrame()->renderStyle();
        if (iframeStyle)
            documentStyle->inheritFrom(iframeStyle);
    }

    // FIXME: It's not clear which values below we want to override in the seamless case!
    documentStyle->setDisplay(BLOCK);
    if (!seamlessWithParent) {
        documentStyle->setRTLOrdering(document.visuallyOrdered() ? VisualOrder : LogicalOrder);
        documentStyle->setZoom(frame && !document.printing() ? frame->pageZoomFactor() : 1);
        documentStyle->setLocale(document.contentLanguage());
    }
    // This overrides any -webkit-user-modify inherited from the parent iframe.
    documentStyle->setUserModify(document.inDesignMode() ? READ_WRITE : READ_ONLY);

    document.setStyleDependentState(documentStyle.get());
    return documentStyle.release();
}

// FIXME: This is duplicated with StyleAdjuster.cpp
// Perhaps this should move onto ElementResolveContext or even Element?
static inline bool isAtShadowBoundary(const Element* element)
{
    if (!element)
        return false;
    ContainerNode* parentNode = element->parentNode();
    return parentNode && parentNode->isShadowRoot();
}

static inline void resetDirectionAndWritingModeOnDocument(Document& document)
{
    document.setDirectionSetOnDocumentElement(false);
    document.setWritingModeSetOnDocumentElement(false);
}

static void addContentAttrValuesToFeatures(const Vector<AtomicString>& contentAttrValues, RuleFeatureSet& features)
{
    for (size_t i = 0; i < contentAttrValues.size(); ++i)
        features.attrsInRules.add(contentAttrValues[i].impl());
}

PassRefPtr<RenderStyle> StyleResolver::styleForElement(Element* element, RenderStyle* defaultParent, StyleSharingBehavior sharingBehavior,
    RuleMatchingBehavior matchingBehavior, RenderRegion* regionForStyling)
{
    ASSERT(document().frame());
    ASSERT(documentSettings());

    // Once an element has a renderer, we don't try to destroy it, since otherwise the renderer
    // will vanish if a style recalc happens during loading.
    if (sharingBehavior == AllowStyleSharing && !element->document().haveStylesheetsLoaded() && !element->renderer()) {
        if (!s_styleNotYetAvailable) {
            s_styleNotYetAvailable = RenderStyle::create().leakRef();
            s_styleNotYetAvailable->setDisplay(NONE);
            s_styleNotYetAvailable->font().update(m_fontSelector);
        }
        element->document().setHasNodesWithPlaceholderStyle();
        return s_styleNotYetAvailable;
    }

    if (element == document().documentElement())
        resetDirectionAndWritingModeOnDocument(document());
    StyleResolverState state(document(), element, defaultParent, regionForStyling);

    if (sharingBehavior == AllowStyleSharing && !state.distributedToInsertionPoint() && state.parentStyle()) {
        SharedStyleFinder styleFinder(state.elementContext(), m_features, m_siblingRuleSet.get(), m_uncommonAttributeRuleSet.get(), *this);
        if (RefPtr<RenderStyle> sharedStyle = styleFinder.findSharedStyle())
            return sharedStyle.release();
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

    state.fontBuilder().initForStyleResolve(state.document(), state.style(), state.useSVGZoomRules());

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

    {
        ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
        collector.setRegionForStyling(regionForStyling);

        if (matchingBehavior == MatchOnlyUserAgentRules)
            matchUARules(collector);
        else
            matchAllRules(state, collector, m_matchAuthorAndUserStyles, matchingBehavior != MatchAllRulesExcludingSMIL);

        applyMatchedProperties(state, collector.matchedResult());

        addContentAttrValuesToFeatures(state.contentAttrValues(), m_features);
    }
    {
        StyleAdjuster adjuster(state.cachedUAStyle(), m_document.inQuirksMode());
        adjuster.adjustRenderStyle(state.style(), state.parentStyle(), element);
    }

    document().didAccessStyleResolver();

    // FIXME: Shouldn't this be on RenderBody::styleDidChange?
    if (element->hasTagName(bodyTag))
        document().textLinkColors().setTextColor(state.style()->visitedDependentColor(CSSPropertyColor));

    // If any changes to CSS Animations were detected, stash the update away for application after the
    // render object is updated if we're in the appropriate scope.
    if (RuntimeEnabledFeatures::webAnimationsCSSEnabled() && state.animationUpdate())
        element->ensureActiveAnimations()->cssAnimations().setPendingUpdate(state.takeAnimationUpdate());

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForKeyframe(Element* e, const RenderStyle* elementStyle, const StyleKeyframe* keyframe)
{
    ASSERT(document().frame());
    ASSERT(documentSettings());

    if (e == document().documentElement())
        resetDirectionAndWritingModeOnDocument(document());
    StyleResolverState state(document(), e);

    MatchResult result;
    if (keyframe->properties())
        result.addMatchedProperties(keyframe->properties());

    ASSERT(!state.style());

    // Create the style
    state.setStyle(RenderStyle::clone(elementStyle));
    state.setLineHeightValue(0);

    state.fontBuilder().initForStyleResolve(state.document(), state.style(), state.useSVGZoomRules());

    // We don't need to bother with !important. Since there is only ever one
    // decl, there's nothing to override. So just add the first properties.
    bool inheritedOnly = false;
    if (keyframe->properties()) {
        // FIXME: Can't keyframes contain variables?
        applyMatchedProperties<AnimationProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);
        applyMatchedProperties<HighPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);
    }

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    // Line-height is set when we are sure we decided on the font-size
    if (state.lineHeightValue())
        StyleBuilder::applyProperty(CSSPropertyLineHeight, state, state.lineHeightValue());

    // Now do rest of the properties.
    if (keyframe->properties())
        applyMatchedProperties<LowPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied by one of the non-essential font props,
    // go ahead and update it a second time.
    updateFont(state);

    // Start loading resources referenced by this style.
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());

    document().didAccessStyleResolver();

    return state.takeStyle();
}

const StyleRuleKeyframes* StyleResolver::matchScopedKeyframesRule(const Element* e, const StringImpl* animationName)
{
    if (m_styleTree.hasOnlyScopedResolverForDocument())
        return m_styleTree.scopedStyleResolverForDocument()->keyframeStylesForAnimation(animationName);

    Vector<ScopedStyleResolver*, 8> stack;
    m_styleTree.resolveScopedKeyframesRules(e, stack);
    if (stack.isEmpty())
        return 0;

    for (size_t i = 0; i < stack.size(); ++i) {
        if (const StyleRuleKeyframes* keyframesRule = stack.at(i)->keyframeStylesForAnimation(animationName))
            return keyframesRule;
    }
    return 0;
}

void StyleResolver::keyframeStylesForAnimation(Element* e, const RenderStyle* elementStyle, KeyframeList& list)
{
    ASSERT(!RuntimeEnabledFeatures::webAnimationsCSSEnabled());
    list.clear();

    // Get the keyframesRule for this name
    if (!e || list.animationName().isEmpty())
        return;

    const StyleRuleKeyframes* keyframesRule = matchScopedKeyframesRule(e, list.animationName().impl());
    if (!keyframesRule)
        return;

    // Construct and populate the style for each keyframe
    const Vector<RefPtr<StyleKeyframe> >& keyframes = keyframesRule->keyframes();
    for (unsigned i = 0; i < keyframes.size(); ++i) {
        // Apply the declaration to the style. This is a simplified version of the logic in styleForElement
        const StyleKeyframe* keyframe = keyframes[i].get();

        KeyframeValue keyframeValue(0, 0);
        keyframeValue.setStyle(styleForKeyframe(e, elementStyle, keyframe));
        keyframeValue.addProperties(keyframe->properties());

        // Add this keyframe style to all the indicated key times
        const Vector<double>& keys = keyframe->keys();
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
        keyframeValue.setStyle(styleForKeyframe(e, elementStyle, zeroPercentKeyframe));
        keyframeValue.addProperties(zeroPercentKeyframe->properties());
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
        keyframeValue.setStyle(styleForKeyframe(e, elementStyle, hundredPercentKeyframe));
        keyframeValue.addProperties(hundredPercentKeyframe->properties());
        list.insert(keyframeValue);
    }
}

void StyleResolver::resolveKeyframes(Element* element, const RenderStyle* style, const AtomicString& name, TimingFunction* defaultTimingFunction, Vector<std::pair<KeyframeAnimationEffect::KeyframeVector, RefPtr<TimingFunction> > >& keyframesAndTimingFunctions)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsCSSEnabled());
    const StyleRuleKeyframes* keyframesRule = matchScopedKeyframesRule(element, name.impl());
    if (!keyframesRule)
        return;

    const Vector<RefPtr<StyleKeyframe> >& styleKeyframes = keyframesRule->keyframes();
    if (styleKeyframes.isEmpty())
        return;

    // Construct and populate the style for each keyframe
    KeyframeAnimationEffect::KeyframeVector keyframes;
    HashMap<double, RefPtr<TimingFunction> > perKeyframeTimingFunctions;
    for (size_t i = 0; i < styleKeyframes.size(); ++i) {
        const StyleKeyframe* styleKeyframe = styleKeyframes[i].get();
        RefPtr<RenderStyle> keyframeStyle = styleForKeyframe(element, style, styleKeyframe);
        RefPtr<Keyframe> keyframe = Keyframe::create();
        const Vector<double>& offsets = styleKeyframe->keys();
        ASSERT(!offsets.isEmpty());
        keyframe->setOffset(offsets[0]);
        TimingFunction* timingFunction = defaultTimingFunction;
        const StylePropertySet* properties = styleKeyframe->properties();
        for (unsigned j = 0; j < properties->propertyCount(); j++) {
            CSSPropertyID property = properties->propertyAt(j).id();
            if (property == CSSPropertyWebkitAnimationTimingFunction || property == CSSPropertyAnimationTimingFunction) {
                // FIXME: This sometimes gets the wrong timing function. See crbug.com/288540.
                timingFunction = KeyframeValue::timingFunction(keyframeStyle.get(), name);
            } else if (CSSAnimations::isAnimatableProperty(property)) {
                keyframe->setPropertyValue(property, CSSAnimatableValueFactory::create(property, keyframeStyle.get()).get());
            }
        }
        keyframes.append(keyframe);
        // The last keyframe specified at a given offset is used.
        perKeyframeTimingFunctions.set(offsets[0], timingFunction);
        for (size_t j = 1; j < offsets.size(); ++j) {
            keyframes.append(keyframe->cloneWithOffset(offsets[j]));
            perKeyframeTimingFunctions.set(offsets[j], timingFunction);
        }
    }
    ASSERT(!keyframes.isEmpty());

    if (!perKeyframeTimingFunctions.contains(0))
        perKeyframeTimingFunctions.set(0, defaultTimingFunction);

    // Remove duplicate keyframes. In CSS the last keyframe at a given offset takes priority.
    std::stable_sort(keyframes.begin(), keyframes.end(), Keyframe::compareOffsets);
    size_t targetIndex = 0;
    for (size_t i = 1; i < keyframes.size(); i++) {
        if (keyframes[i]->offset() != keyframes[targetIndex]->offset())
            targetIndex++;
        if (targetIndex != i)
            keyframes[targetIndex] = keyframes[i];
    }
    keyframes.shrink(targetIndex + 1);

    // Add 0% and 100% keyframes if absent.
    RefPtr<Keyframe> startKeyframe = keyframes[0];
    if (startKeyframe->offset()) {
        startKeyframe = Keyframe::create();
        startKeyframe->setOffset(0);
        keyframes.prepend(startKeyframe);
    }
    RefPtr<Keyframe> endKeyframe = keyframes[keyframes.size() - 1];
    if (endKeyframe->offset() != 1) {
        endKeyframe = Keyframe::create();
        endKeyframe->setOffset(1);
        keyframes.append(endKeyframe);
    }
    ASSERT(keyframes.size() >= 2);
    ASSERT(!keyframes.first()->offset());
    ASSERT(keyframes.last()->offset() == 1);

    // Snapshot current property values for 0% and 100% if missing.
    PropertySet allProperties;
    size_t numKeyframes = keyframes.size();
    for (size_t i = 0; i < numKeyframes; i++) {
        const PropertySet& keyframeProperties = keyframes[i]->properties();
        for (PropertySet::const_iterator iter = keyframeProperties.begin(); iter != keyframeProperties.end(); ++iter)
            allProperties.add(*iter);
    }
    const PropertySet& startKeyframeProperties = startKeyframe->properties();
    const PropertySet& endKeyframeProperties = endKeyframe->properties();
    bool missingStartValues = startKeyframeProperties.size() < allProperties.size();
    bool missingEndValues = endKeyframeProperties.size() < allProperties.size();
    if (missingStartValues || missingEndValues) {
        for (PropertySet::const_iterator iter = allProperties.begin(); iter != allProperties.end(); ++iter) {
            const CSSPropertyID property = *iter;
            bool startNeedsValue = missingStartValues && !startKeyframeProperties.contains(property);
            bool endNeedsValue = missingEndValues && !endKeyframeProperties.contains(property);
            if (!startNeedsValue && !endNeedsValue)
                continue;
            RefPtr<AnimatableValue> snapshotValue = CSSAnimatableValueFactory::create(property, style);
            if (startNeedsValue)
                startKeyframe->setPropertyValue(property, snapshotValue.get());
            if (endNeedsValue)
                endKeyframe->setPropertyValue(property, snapshotValue.get());
        }
    }
    ASSERT(startKeyframe->properties().size() == allProperties.size());
    ASSERT(endKeyframe->properties().size() == allProperties.size());

    // Determine how many keyframes specify each property. Note that this must
    // be done after we've filled in end keyframes.
    typedef HashCountedSet<CSSPropertyID> PropertyCountedSet;
    PropertyCountedSet propertyCounts;
    for (size_t i = 0; i < numKeyframes; ++i) {
        const PropertySet& properties = keyframes[i]->properties();
        for (PropertySet::const_iterator iter = properties.begin(); iter != properties.end(); ++iter)
            propertyCounts.add(*iter);
    }

    // Split keyframes into groups, where each group contains only keyframes
    // which specify all properties used in that group. Each group is animated
    // in a separate animation, to allow per-keyframe timing functions to be
    // applied correctly.
    for (PropertyCountedSet::const_iterator iter = propertyCounts.begin(); iter != propertyCounts.end(); ++iter) {
        const CSSPropertyID property = iter->key;
        const size_t count = iter->value;
        ASSERT(count <= numKeyframes);
        if (count == numKeyframes)
            continue;
        KeyframeAnimationEffect::KeyframeVector splitOutKeyframes;
        for (size_t i = 0; i < numKeyframes; i++) {
            Keyframe* keyframe = keyframes[i].get();
            if (!keyframe->properties().contains(property)) {
                ASSERT(i && i != numKeyframes - 1);
                continue;
            }
            RefPtr<Keyframe> clonedKeyframe = Keyframe::create();
            clonedKeyframe->setOffset(keyframe->offset());
            clonedKeyframe->setComposite(keyframe->composite());
            clonedKeyframe->setPropertyValue(property, keyframe->propertyValue(property));
            splitOutKeyframes.append(clonedKeyframe);
            // Note that it's OK if this keyframe ends up having no
            // properties. This can only happen when none of the properties
            // are specified in all keyframes, in which case we won't animate
            // anything with these keyframes.
            keyframe->clearPropertyValue(property);
        }
        ASSERT(!splitOutKeyframes.first()->offset());
        ASSERT(splitOutKeyframes.last()->offset() == 1);
#ifndef NDEBUG
        for (size_t j = 0; j < splitOutKeyframes.size(); ++j)
            ASSERT(splitOutKeyframes[j]->properties().size() == 1);
#endif
        keyframesAndTimingFunctions.append(std::make_pair(splitOutKeyframes, generateTimingFunction(splitOutKeyframes, perKeyframeTimingFunctions)));
    }

    int numPropertiesSpecifiedInAllKeyframes = keyframes.first()->properties().size();
#ifndef NDEBUG
    for (size_t i = 1; i < numKeyframes; ++i)
        ASSERT(keyframes[i]->properties().size() == numPropertiesSpecifiedInAllKeyframes);
#endif

    // If the animation specifies any keyframes, we always provide at least one
    // vector of resolved keyframes, even if no properties are animated.
    if (numPropertiesSpecifiedInAllKeyframes || keyframesAndTimingFunctions.isEmpty())
        keyframesAndTimingFunctions.append(std::make_pair(keyframes, generateTimingFunction(keyframes, perKeyframeTimingFunctions)));
}

PassRefPtr<RenderStyle> StyleResolver::pseudoStyleForElement(Element* e, const PseudoStyleRequest& pseudoStyleRequest, RenderStyle* parentStyle)
{
    ASSERT(document().frame());
    ASSERT(documentSettings());
    ASSERT(parentStyle);
    if (!e)
        return 0;

    StyleResolverState state(document(), e, parentStyle);

    if (pseudoStyleRequest.allowsInheritance(state.parentStyle())) {
        state.setStyle(RenderStyle::create());
        state.style()->inheritFrom(state.parentStyle());
    } else {
        state.setStyle(defaultStyleForElement());
        state.setParentStyle(RenderStyle::clone(state.style()));
    }

    state.fontBuilder().initForStyleResolve(state.document(), state.style(), state.useSVGZoomRules());

    // Since we don't use pseudo-elements in any of our quirk/print
    // user agent rules, don't waste time walking those rules.

    {
        // Check UA, user and author rules.
        ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
        collector.setPseudoStyleRequest(pseudoStyleRequest);

        matchUARules(collector);
        if (m_matchAuthorAndUserStyles) {
            matchUserRules(collector, false);
            matchAuthorRules(state.element(), collector, false);
        }

        if (collector.matchedResult().matchedProperties.isEmpty())
            return 0;

        state.style()->setStyleType(pseudoStyleRequest.pseudoId);

        applyMatchedProperties(state, collector.matchedResult());

        addContentAttrValuesToFeatures(state.contentAttrValues(), m_features);
    }
    {
        StyleAdjuster adjuster(state.cachedUAStyle(), m_document.inQuirksMode());
        // FIXME: Passing 0 as the Element* introduces a lot of complexity
        // in the adjustRenderStyle code.
        adjuster.adjustRenderStyle(state.style(), state.parentStyle(), 0);
    }

    document().didAccessStyleResolver();

    // Now return the style.
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForPage(int pageIndex)
{
    resetDirectionAndWritingModeOnDocument(document());
    StyleResolverState state(document(), document().documentElement()); // m_rootElementStyle will be set to the document style.

    state.setStyle(RenderStyle::create());
    const RenderStyle* rootElementStyle = state.rootElementStyle() ? state.rootElementStyle() : document().renderStyle();
    ASSERT(rootElementStyle);
    state.style()->inheritFrom(rootElementStyle);

    state.fontBuilder().initForStyleResolve(state.document(), state.style(), state.useSVGZoomRules());

    PageRuleCollector collector(rootElementStyle, pageIndex);

    collector.matchPageRules(CSSDefaultStyleSheets::defaultPrintStyle);
    collector.matchPageRules(m_ruleSets.userStyle());

    if (ScopedStyleResolver* scopedResolver = m_styleTree.scopedStyleResolverForDocument())
        scopedResolver->matchPageRules(collector);

    state.setLineHeightValue(0);
    bool inheritedOnly = false;

    MatchResult& result = collector.matchedResult();
    applyMatchedProperties<VariableDefinitions>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    // Line-height is set when we are sure we decided on the font-size.
    if (state.lineHeightValue())
        StyleBuilder::applyProperty(CSSPropertyLineHeight, state, state.lineHeightValue());

    applyMatchedProperties<LowPriorityProperties>(state, result, false, 0, result.matchedProperties.size() - 1, inheritedOnly);

    addContentAttrValuesToFeatures(state.contentAttrValues(), m_features);

    // Start loading resources referenced by this style.
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());

    document().didAccessStyleResolver();

    // Now return the style.
    return state.takeStyle();
}

void StyleResolver::collectViewportRules()
{
    viewportStyleResolver()->collectViewportRules(CSSDefaultStyleSheets::defaultStyle, ViewportStyleResolver::UserAgentOrigin);

    if (document().isMobileDocument())
        viewportStyleResolver()->collectViewportRules(CSSDefaultStyleSheets::xhtmlMobileProfileStyle(), ViewportStyleResolver::UserAgentOrigin);

    if (m_ruleSets.userStyle())
        viewportStyleResolver()->collectViewportRules(m_ruleSets.userStyle(), ViewportStyleResolver::UserAgentOrigin);

    if (ScopedStyleResolver* scopedResolver = m_styleTree.scopedStyleResolverForDocument())
        scopedResolver->collectViewportRulesTo(this);

    viewportStyleResolver()->resolve();
}

PassRefPtr<RenderStyle> StyleResolver::defaultStyleForElement()
{
    StyleResolverState state(document(), 0);
    state.setStyle(RenderStyle::create());
    state.fontBuilder().initForStyleResolve(document(), state.style(), state.useSVGZoomRules());
    state.style()->setLineHeight(RenderStyle::initialLineHeight());
    state.setLineHeightValue(0);
    state.fontBuilder().setInitial(state.style()->effectiveZoom());
    state.style()->font().update(fontSelector());
    return state.takeStyle();
}

PassRefPtr<RenderStyle> StyleResolver::styleForText(Text* textNode)
{
    ASSERT(textNode);

    NodeRenderingTraversal::ParentDetails parentDetails;
    Node* parentNode = NodeRenderingTraversal::parent(textNode, &parentDetails);
    if (!parentNode || !parentNode->renderStyle() || parentDetails.resetStyleInheritance())
        return defaultStyleForElement();
    return parentNode->renderStyle();
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

void StyleResolver::updateFont(StyleResolverState& state)
{
    state.fontBuilder().createFont(m_fontSelector, state.parentStyle(), state.style());
}

PassRefPtr<StyleRuleList> StyleResolver::styleRulesForElement(Element* element, unsigned rulesToInclude)
{
    ASSERT(element);
    StyleResolverState state(document(), element);
    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style());
    collector.setMode(SelectorChecker::CollectingStyleRules);
    collectPseudoRulesForElement(element, collector, NOPSEUDO, rulesToInclude);
    return collector.matchedStyleRuleList();
}

PassRefPtr<CSSRuleList> StyleResolver::pseudoCSSRulesForElement(Element* element, PseudoId pseudoId, unsigned rulesToInclude, ShouldIncludeStyleSheetInCSSOMWrapper includeDocument)
{
    ASSERT(element);
    StyleResolverState state(document(), element);
    ElementRuleCollector collector(state.elementContext(), m_selectorFilter, state.style(), includeDocument);
    collector.setMode(SelectorChecker::CollectingCSSRules);
    collectPseudoRulesForElement(element, collector, pseudoId, rulesToInclude);
    return collector.matchedCSSRuleList();
}

PassRefPtr<CSSRuleList> StyleResolver::cssRulesForElement(Element* element, unsigned rulesToInclude, ShouldIncludeStyleSheetInCSSOMWrapper includeDocument)
{
    return pseudoCSSRulesForElement(element, NOPSEUDO, rulesToInclude, includeDocument);
}

void StyleResolver::collectPseudoRulesForElement(Element* element, ElementRuleCollector& collector, PseudoId pseudoId, unsigned rulesToInclude)
{
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
        matchAuthorRules(element, collector, rulesToInclude & EmptyCSSRules);
    }
}

// -------------------------------------------------------------------------------------
// this is mostly boring stuff on how to apply a certain rule to the renderstyle...

template <StyleResolver::StyleApplicationPass pass>
bool StyleResolver::applyAnimatedProperties(StyleResolverState& state, const AnimationEffect::CompositableValueMap& compositableValues)
{
    ASSERT(RuntimeEnabledFeatures::webAnimationsCSSEnabled());
    ASSERT(pass != VariableDefinitions);
    ASSERT(pass != AnimationProperties);
    bool didApply = false;

    for (AnimationEffect::CompositableValueMap::const_iterator iter = compositableValues.begin(); iter != compositableValues.end(); ++iter) {
        CSSPropertyID property = iter->key;
        if (!isPropertyForPass<pass>(property))
            continue;
        RELEASE_ASSERT_WITH_MESSAGE(!iter->value->dependsOnUnderlyingValue(), "Web Animations not yet implemented: An interface for compositing onto the underlying value.");
        RefPtr<AnimatableValue> animatableValue = iter->value->compositeOnto(0);
        AnimatedStyleBuilder::applyProperty(property, state, animatableValue.get());
        didApply = true;
    }
    return didApply;
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
    // FIXME: 'text-decoration' shorthand to be handled when available.
    // See https://chromiumcodereview.appspot.com/19516002 for details.
    case CSSPropertyTextDecoration:
    case CSSPropertyTextShadow:
    case CSSPropertyBorderStyle:
        return true;
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextDecorationColor:
        return RuntimeEnabledFeatures::css3TextDecorationsEnabled();
    default:
        break;
    }
    return false;
}

template <StyleResolver::StyleApplicationPass pass>
bool StyleResolver::isPropertyForPass(CSSPropertyID property)
{
    COMPILE_ASSERT(CSSPropertyVariable < firstCSSProperty, CSS_variable_is_before_first_property);
    const CSSPropertyID firstAnimationProperty = CSSPropertyDisplay;
    const CSSPropertyID lastAnimationProperty = CSSPropertyTransitionTimingFunction;
    COMPILE_ASSERT(firstCSSProperty == firstAnimationProperty, CSS_first_animation_property_should_be_first_property);
    const CSSPropertyID firstHighPriorityProperty = CSSPropertyColor;
    const CSSPropertyID lastHighPriorityProperty = CSSPropertyLineHeight;
    COMPILE_ASSERT(lastAnimationProperty + 1 == firstHighPriorityProperty, CSS_color_is_first_high_priority_property);
    COMPILE_ASSERT(CSSPropertyLineHeight == firstHighPriorityProperty + 17, CSS_line_height_is_end_of_high_prioity_property_range);
    COMPILE_ASSERT(CSSPropertyZoom == lastHighPriorityProperty - 1, CSS_zoom_is_before_line_height);
    switch (pass) {
    case VariableDefinitions:
        return property == CSSPropertyVariable;
    case AnimationProperties:
        return property >= firstAnimationProperty && property <= lastAnimationProperty;
    case HighPriorityProperties:
        return property >= firstHighPriorityProperty && property <= lastHighPriorityProperty;
    case LowPriorityProperties:
        return property > lastHighPriorityProperty;
    }
    ASSERT_NOT_REACHED();
    return false;
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyProperties(StyleResolverState& state, const StylePropertySet* properties, StyleRule* rule, bool isImportant, bool inheritedOnly, PropertyWhitelistType propertyWhitelistType)
{
    ASSERT((propertyWhitelistType != PropertyWhitelistRegion) || state.regionForStyling());
    state.setCurrentRule(rule);

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
        if (!isPropertyForPass<pass>(property))
            continue;
        if (pass == HighPriorityProperties && property == CSSPropertyLineHeight)
            state.setLineHeightValue(current.value());
        else
            StyleBuilder::applyProperty(current.id(), state, current.value());
    }
}

template <StyleResolver::StyleApplicationPass pass>
void StyleResolver::applyMatchedProperties(StyleResolverState& state, const MatchResult& matchResult, bool isImportant, int startIndex, int endIndex, bool inheritedOnly)
{
    if (startIndex == -1)
        return;

    if (state.style()->insideLink() != NotInsideLink) {
        for (int i = startIndex; i <= endIndex; ++i) {
            const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
            unsigned linkMatchType = matchedProperties.linkMatchType;
            // FIXME: It would be nicer to pass these as arguments but that requires changes in many places.
            state.setApplyPropertyToRegularStyle(linkMatchType & SelectorChecker::MatchLink);
            state.setApplyPropertyToVisitedLinkStyle(linkMatchType & SelectorChecker::MatchVisited);

            applyProperties<pass>(state, matchedProperties.properties.get(), matchResult.matchedRules[i], isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.whitelistType));
        }
        state.setApplyPropertyToRegularStyle(true);
        state.setApplyPropertyToVisitedLinkStyle(false);
        return;
    }
    for (int i = startIndex; i <= endIndex; ++i) {
        const MatchedProperties& matchedProperties = matchResult.matchedProperties[i];
        applyProperties<pass>(state, matchedProperties.properties.get(), matchResult.matchedRules[i], isImportant, inheritedOnly, static_cast<PropertyWhitelistType>(matchedProperties.whitelistType));
    }
}

static unsigned computeMatchedPropertiesHash(const MatchedProperties* properties, unsigned size)
{
    return StringHasher::hashMemory(properties, sizeof(MatchedProperties) * size);
}

void StyleResolver::invalidateMatchedPropertiesCache()
{
    m_matchedPropertiesCache.clear();
}

void StyleResolver::applyMatchedProperties(StyleResolverState& state, const MatchResult& matchResult)
{
    const Element* element = state.element();
    ASSERT(element);
    STYLE_STATS_ADD_MATCHED_PROPERTIES_SEARCH();

    unsigned cacheHash = matchResult.isCacheable ? computeMatchedPropertiesHash(matchResult.matchedProperties.data(), matchResult.matchedProperties.size()) : 0;
    bool applyInheritedOnly = false;
    const CachedMatchedProperties* cachedMatchedProperties = 0;

    if (cacheHash && (cachedMatchedProperties = m_matchedPropertiesCache.find(cacheHash, state, matchResult))
        && MatchedPropertiesCache::isCacheable(element, state.style(), state.parentStyle())) {
        STYLE_STATS_ADD_MATCHED_PROPERTIES_HIT();
        // We can build up the style by copying non-inherited properties from an earlier style object built using the same exact
        // style declarations. We then only need to apply the inherited properties, if any, as their values can depend on the
        // element context. This is fast and saves memory by reusing the style data structures.
        state.style()->copyNonInheritedFrom(cachedMatchedProperties->renderStyle.get());
        if (state.parentStyle()->inheritedDataShared(cachedMatchedProperties->parentRenderStyle.get()) && !isAtShadowBoundary(element)) {
            STYLE_STATS_ADD_MATCHED_PROPERTIES_HIT_SHARED_INHERITED();

            EInsideLink linkStatus = state.style()->insideLink();
            // If the cache item parent style has identical inherited properties to the current parent style then the
            // resulting style will be identical too. We copy the inherited properties over from the cache and are done.
            state.style()->inheritFrom(cachedMatchedProperties->renderStyle.get());

            // Unfortunately the link status is treated like an inherited property. We need to explicitly restore it.
            state.style()->setInsideLink(linkStatus);
            return;
        }
        applyInheritedOnly = true;
    }

    // First apply all variable definitions, as they may be used during application of later properties.
    applyMatchedProperties<VariableDefinitions>(state, matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<VariableDefinitions>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<VariableDefinitions>(state, matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<VariableDefinitions>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Apply animation properties in order to apply animation results and trigger transitions below.
    applyMatchedProperties<AnimationProperties>(state, matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<AnimationProperties>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<AnimationProperties>(state, matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<AnimationProperties>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Match transition-property / animation-name length by trimming and
    // lengthening other transition / animation property lists
    // FIXME: This is wrong because we shouldn't affect the computed values
    state.style()->adjustAnimations();
    state.style()->adjustTransitions();

    // Now we have all of the matched rules in the appropriate order. Walk the rules and apply
    // high-priority properties first, i.e., those properties that other properties depend on.
    // The order is (1) high-priority not important, (2) high-priority important, (3) normal not important
    // and (4) normal important.
    state.setLineHeightValue(0);
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, false, 0, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<HighPriorityProperties>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    if (cachedMatchedProperties && cachedMatchedProperties->renderStyle->effectiveZoom() != state.style()->effectiveZoom()) {
        state.fontBuilder().setFontDirty(true);
        applyInheritedOnly = false;
    }

    // If our font got dirtied, go ahead and update it now.
    updateFont(state);

    // Line-height is set when we are sure we decided on the font-size.
    if (state.lineHeightValue())
        StyleBuilder::applyProperty(CSSPropertyLineHeight, state, state.lineHeightValue());

    // Many properties depend on the font. If it changes we just apply all properties.
    if (cachedMatchedProperties && cachedMatchedProperties->renderStyle->fontDescription() != state.style()->fontDescription())
        applyInheritedOnly = false;

    // Now do the normal priority UA properties.
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, false, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    // Cache the UA properties to pass them to RenderTheme in adjustRenderStyle.
    state.cacheUserAgentBorderAndBackground();

    // Now do the author and user normal priority properties and all the !important properties.
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, false, matchResult.ranges.lastUARule + 1, matchResult.matchedProperties.size() - 1, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, true, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, true, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
    applyMatchedProperties<LowPriorityProperties>(state, matchResult, true, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);

    if (RuntimeEnabledFeatures::webAnimationsEnabled()) {
        state.setAnimationUpdate(CSSAnimations::calculateUpdate(state.element(), state.style(), this));
        if (state.animationUpdate()) {
            ASSERT(!applyInheritedOnly);
            const AnimationEffect::CompositableValueMap& compositableValuesForAnimations = state.animationUpdate()->compositableValuesForAnimations();
            const AnimationEffect::CompositableValueMap& compositableValuesForTransitions = state.animationUpdate()->compositableValuesForTransitions();
            // Apply animated properties, then reapply any rules marked important.
            if (applyAnimatedProperties<HighPriorityProperties>(state, compositableValuesForAnimations)) {
                bool important = true;
                applyMatchedProperties<HighPriorityProperties>(state, matchResult, important, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
                applyMatchedProperties<HighPriorityProperties>(state, matchResult, important, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
                applyMatchedProperties<HighPriorityProperties>(state, matchResult, important, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);
            }
            applyAnimatedProperties<HighPriorityProperties>(state, compositableValuesForTransitions);
            if (applyAnimatedProperties<LowPriorityProperties>(state, compositableValuesForAnimations)) {
                bool important = true;
                applyMatchedProperties<LowPriorityProperties>(state, matchResult, important, matchResult.ranges.firstAuthorRule, matchResult.ranges.lastAuthorRule, applyInheritedOnly);
                applyMatchedProperties<LowPriorityProperties>(state, matchResult, important, matchResult.ranges.firstUserRule, matchResult.ranges.lastUserRule, applyInheritedOnly);
                applyMatchedProperties<LowPriorityProperties>(state, matchResult, important, matchResult.ranges.firstUARule, matchResult.ranges.lastUARule, applyInheritedOnly);
            }
            applyAnimatedProperties<LowPriorityProperties>(state, compositableValuesForTransitions);
        }
    }

    // Start loading resources referenced by this style.
    m_styleResourceLoader.loadPendingResources(state.style(), state.elementStyleResources());

    ASSERT(!state.fontBuilder().fontDirty());

#ifdef STYLE_STATS
    if (!cachedMatchedProperties)
        STYLE_STATS_ADD_MATCHED_PROPERTIES_TO_CACHE();
#endif

    if (cachedMatchedProperties || !cacheHash)
        return;
    if (!MatchedPropertiesCache::isCacheable(element, state.style(), state.parentStyle()))
        return;
    STYLE_STATS_ADD_MATCHED_PROPERTIES_ENTERED_INTO_CACHE();
    m_matchedPropertiesCache.add(state.style(), state.parentStyle(), cacheHash, matchResult);
}

CSSPropertyValue::CSSPropertyValue(CSSPropertyID id, const StylePropertySet& propertySet)
    : property(id), value(propertySet.getPropertyCSSValue(id).get())
{ }

void StyleResolver::applyPropertiesToStyle(const CSSPropertyValue* properties, size_t count, RenderStyle* style)
{
    StyleResolverState state(document(), document().documentElement(), style);
    state.setStyle(style);

    state.fontBuilder().initForStyleResolve(document(), style, state.useSVGZoomRules());

    for (size_t i = 0; i < count; ++i) {
        if (properties[i].value) {
            // As described in BUG66291, setting font-size and line-height on a font may entail a CSSPrimitiveValue::computeLengthDouble call,
            // which assumes the fontMetrics are available for the affected font, otherwise a crash occurs (see http://trac.webkit.org/changeset/96122).
            // The updateFont() call below updates the fontMetrics and ensure the proper setting of font-size and line-height.
            switch (properties[i].property) {
            case CSSPropertyFontSize:
            case CSSPropertyLineHeight:
                updateFont(state);
                break;
            default:
                break;
            }
            StyleBuilder::applyProperty(properties[i].property, state, properties[i].value);
        }
    }
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

#ifdef STYLE_STATS
StyleSharingStats StyleResolver::m_styleSharingStats;

static void printStyleStats(unsigned searches, unsigned elementsEligibleForSharing, unsigned stylesShared, unsigned searchFoundSiblingForSharing, unsigned searchesMissedSharing,
    unsigned matchedPropertiesSearches, unsigned matchedPropertiesHit, unsigned matchedPropertiesSharedInheritedHit, unsigned matchedPropertiesToCache, unsigned matchedPropertiesEnteredIntoCache)
{
    double percentOfElementsSharingStyle = (stylesShared * 100.0) / searches;
    double percentOfNodesEligibleForSharing = (elementsEligibleForSharing * 100.0) / searches;
    double percentOfEligibleSharingRelativesFound = (searchFoundSiblingForSharing * 100.0) / searches;
    double percentOfMatchedPropertiesHit = (matchedPropertiesHit * 100.0) / matchedPropertiesSearches;
    double percentOfMatchedPropertiesSharedInheritedHit = (matchedPropertiesSharedInheritedHit * 100.0) / matchedPropertiesSearches;
    double percentOfMatchedPropertiesEnteredIntoCache = (matchedPropertiesEnteredIntoCache * 100.0) / matchedPropertiesToCache;

    fprintf(stderr, "%u elements checked, %u were eligible for style sharing (%.2f%%).\n", searches, elementsEligibleForSharing, percentOfNodesEligibleForSharing);
    fprintf(stderr, "%u elements were found to share with, %u were possible (%.2f%%).\n", searchFoundSiblingForSharing, searchesMissedSharing + searchFoundSiblingForSharing, percentOfEligibleSharingRelativesFound);
    fprintf(stderr, "%u styles were actually shared once sibling and attribute rules were considered (%.2f%%).\n", stylesShared, percentOfElementsSharingStyle);
    fprintf(stderr, "%u/%u (%.2f%%) matched property lookups hit the cache.\n", matchedPropertiesHit, matchedPropertiesSearches, percentOfMatchedPropertiesHit);
    fprintf(stderr, "%u/%u (%.2f%%) matched property lookups hit the cache and shared inherited data.\n", matchedPropertiesSharedInheritedHit, matchedPropertiesSearches, percentOfMatchedPropertiesSharedInheritedHit);
    fprintf(stderr, "%u/%u (%.2f%%) matched properties were cacheable\n", matchedPropertiesEnteredIntoCache, matchedPropertiesToCache, percentOfMatchedPropertiesEnteredIntoCache);
}

void StyleSharingStats::printStats() const
{
    fprintf(stderr, "--------------------------------------------------------------------------------\n");
    fprintf(stderr, "This recalc style:\n");
    printStyleStats(m_searches, m_elementsEligibleForSharing, m_stylesShared, m_searchFoundSiblingForSharing, m_searchesMissedSharing,
        m_matchedPropertiesSearches, m_matchedPropertiesHit, m_matchedPropertiesSharedInheritedHit, m_matchedPropertiesToCache, m_matchedPropertiesEnteredIntoCache);

    fprintf(stderr, "Total:\n");
    printStyleStats(m_totalSearches, m_totalElementsEligibleForSharing, m_totalStylesShared, m_totalSearchFoundSiblingForSharing, m_totalSearchesMissedSharing,
        m_totalMatchedPropertiesSearches, m_totalMatchedPropertiesHit, m_totalMatchedPropertiesSharedInheritedHit, m_totalMatchedPropertiesToCache, m_totalMatchedPropertiesEnteredIntoCache);
    fprintf(stderr, "--------------------------------------------------------------------------------\n");
}
#endif

} // namespace WebCore
