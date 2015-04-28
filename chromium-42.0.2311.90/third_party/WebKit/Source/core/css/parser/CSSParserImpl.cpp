// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSParserImpl.h"

#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleRuleKeyframe.h"
#include "core/css/StyleRuleNamespace.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSSelectorParser.h"
#include "core/css/parser/CSSSupportsParser.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/parser/MediaQueryParser.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/UseCounter.h"
#include "wtf/BitArray.h"

namespace blink {

CSSParserImpl::CSSParserImpl(const CSSParserContext& context, const String& string, StyleSheetContents* styleSheet)
: m_context(context)
, m_defaultNamespace(starAtom)
, m_styleSheet(styleSheet)
{
    CSSTokenizer::tokenize(string, m_tokens);
}

bool CSSParserImpl::parseValue(MutableStylePropertySet* declaration, CSSPropertyID propertyID, const String& string, bool important, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    CSSRuleSourceData::Type ruleType = CSSRuleSourceData::STYLE_RULE;
    if (declaration->cssParserMode() == CSSViewportRuleMode)
        ruleType = CSSRuleSourceData::VIEWPORT_RULE;
    parser.consumeDeclarationValue(CSSParserTokenRange(parser.m_tokens), propertyID, important, ruleType);
    if (parser.m_parsedProperties.isEmpty())
        return false;
    declaration->addParsedProperties(parser.m_parsedProperties);
    return true;
}

static inline void filterProperties(bool important, const WillBeHeapVector<CSSProperty, 256>& input, WillBeHeapVector<CSSProperty, 256>& output, size_t& unusedEntries, BitArray<numCSSProperties>& seenProperties)
{
    // Add properties in reverse order so that highest priority definitions are reached first. Duplicate definitions can then be ignored when found.
    for (size_t i = input.size(); i--; ) {
        const CSSProperty& property = input[i];
        if (property.isImportant() != important)
            continue;
        const unsigned propertyIDIndex = property.id() - firstCSSProperty;
        if (seenProperties.get(propertyIDIndex))
            continue;
        seenProperties.set(propertyIDIndex);
        output[--unusedEntries] = property;
    }
}

static PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> createStylePropertySet(const WillBeHeapVector<CSSProperty, 256>& parsedProperties, CSSParserMode mode)
{
    BitArray<numCSSProperties> seenProperties;
    size_t unusedEntries = parsedProperties.size();
    WillBeHeapVector<CSSProperty, 256> results(unusedEntries);

    filterProperties(true, parsedProperties, results, unusedEntries, seenProperties);
    filterProperties(false, parsedProperties, results, unusedEntries, seenProperties);

    return ImmutableStylePropertySet::create(results.data() + unusedEntries, results.size() - unusedEntries, mode);
}

PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> CSSParserImpl::parseInlineStyleDeclaration(const String& string, Element* element)
{
    Document& document = element->document();
    CSSParserContext context = CSSParserContext(document.elementSheet().contents()->parserContext(), UseCounter::getFrom(&document));
    CSSParserMode mode = element->isHTMLElement() && !document.inQuirksMode() ? HTMLStandardMode : HTMLQuirksMode;
    context.setMode(mode);
    CSSParserImpl parser(context, string);
    parser.consumeDeclarationList(CSSParserTokenRange(parser.m_tokens), CSSRuleSourceData::STYLE_RULE);
    return createStylePropertySet(parser.m_parsedProperties, mode);
}

bool CSSParserImpl::parseDeclaration(MutableStylePropertySet* declaration, const String& string, const CSSParserContext& context)
{
    CSSParserImpl parser(context, string);
    CSSRuleSourceData::Type ruleType = CSSRuleSourceData::STYLE_RULE;
    if (declaration->cssParserMode() == CSSViewportRuleMode)
        ruleType = CSSRuleSourceData::VIEWPORT_RULE;
    parser.consumeDeclarationList(CSSParserTokenRange(parser.m_tokens), ruleType);
    if (parser.m_parsedProperties.isEmpty())
        return false;
    declaration->addParsedProperties(parser.m_parsedProperties);
    return true;
}

PassRefPtrWillBeRawPtr<StyleRuleBase> CSSParserImpl::parseRule(const String& string, const CSSParserContext& context, AllowedRulesType allowedRules)
{
    CSSParserImpl parser(context, string);
    CSSParserTokenRange range(parser.m_tokens);
    range.consumeWhitespaceAndComments();
    if (range.atEnd())
        return nullptr; // Parse error, empty rule
    RefPtrWillBeRawPtr<StyleRuleBase> rule;
    if (range.peek().type() == AtKeywordToken)
        rule = parser.consumeAtRule(range, allowedRules);
    else
        rule = parser.consumeQualifiedRule(range, allowedRules);
    if (!rule)
        return nullptr; // Parse error, failed to consume rule
    range.consumeWhitespaceAndComments();
    if (!rule || !range.atEnd())
        return nullptr; // Parse error, trailing garbage
    return rule;
}

void CSSParserImpl::parseStyleSheet(const String& string, const CSSParserContext& context, StyleSheetContents* styleSheet)
{
    CSSParserImpl parser(context, string, styleSheet);
    parser.consumeRuleList(parser.m_tokens, TopLevelRuleList, [&styleSheet](PassRefPtrWillBeRawPtr<StyleRuleBase> rule) {
        styleSheet->parserAppendRule(rule);
    });
}

PassOwnPtr<Vector<double>> CSSParserImpl::parseKeyframeKeyList(const String& keyList)
{
    Vector<CSSParserToken> tokens;
    CSSTokenizer::tokenize(keyList, tokens);
    return consumeKeyframeKeyList(tokens);
}

bool CSSParserImpl::supportsDeclaration(CSSParserTokenRange& range)
{
    ASSERT(m_parsedProperties.isEmpty());
    consumeDeclaration(range, CSSRuleSourceData::STYLE_RULE);
    bool result = !m_parsedProperties.isEmpty();
    m_parsedProperties.clear();
    return result;
}

static CSSParserImpl::AllowedRulesType computeNewAllowedRules(CSSParserImpl::AllowedRulesType allowedRules, StyleRuleBase* rule)
{
    if (!rule || allowedRules == CSSParserImpl::KeyframeRules)
        return allowedRules;
    ASSERT(allowedRules <= CSSParserImpl::RegularRules);
    if (rule->isImportRule())
        return CSSParserImpl::AllowImportRules;
    if (rule->isNamespaceRule())
        return CSSParserImpl::AllowNamespaceRules;
    return CSSParserImpl::RegularRules;
}

template<typename T>
void CSSParserImpl::consumeRuleList(CSSParserTokenRange range, RuleListType ruleListType, const T callback)
{
    AllowedRulesType allowedRules = RegularRules;
    switch (ruleListType) {
    case TopLevelRuleList:
        allowedRules = AllowCharsetRules;
        break;
    case RegularRuleList:
        allowedRules = RegularRules;
        break;
    case KeyframesRuleList:
        allowedRules = KeyframeRules;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    while (!range.atEnd()) {
        switch (range.peek().type()) {
        case WhitespaceToken:
        case CommentToken:
            range.consumeWhitespaceAndComments();
            break;
        case AtKeywordToken:
            if (PassRefPtrWillBeRawPtr<StyleRuleBase> rule = consumeAtRule(range, allowedRules)) {
                allowedRules = computeNewAllowedRules(allowedRules, rule.get());
                callback(rule);
            }
            break;
        case CDOToken:
        case CDCToken:
            if (ruleListType == TopLevelRuleList) {
                range.consume();
                break;
            }
            // fallthrough
        default:
            if (PassRefPtrWillBeRawPtr<StyleRuleBase> rule = consumeQualifiedRule(range, allowedRules)) {
                allowedRules = computeNewAllowedRules(allowedRules, rule.get());
                callback(rule);
            }
            break;
        }
    }
}

PassRefPtrWillBeRawPtr<StyleRuleBase> CSSParserImpl::consumeAtRule(CSSParserTokenRange& range, AllowedRulesType allowedRules)
{
    ASSERT(range.peek().type() == AtKeywordToken);
    const String& name = range.consume().value();
    const CSSParserToken* preludeStart = &range.peek();
    while (!range.atEnd() && range.peek().type() != LeftBraceToken && range.peek().type() != SemicolonToken)
        range.consumeComponentValue();

    CSSParserTokenRange prelude = range.makeSubRange(preludeStart, &range.peek());

    if (range.atEnd() || range.peek().type() == SemicolonToken) {
        range.consume();
        if (allowedRules == AllowCharsetRules && equalIgnoringCase(name, "charset")) {
            // @charset is actually parsed before we get into the CSS parser.
            // In theory we should validate the prelude is a string, but we don't
            // have error logging yet so it doesn't matter.
            return nullptr;
        }
        if (allowedRules <= AllowImportRules && equalIgnoringCase(name, "import"))
            return consumeImportRule(prelude);
        if (allowedRules <= AllowNamespaceRules && equalIgnoringCase(name, "namespace"))
            return consumeNamespaceRule(prelude);
        return nullptr; // Parse error, unrecognised at-rule without block
    }

    CSSParserTokenRange block = range.consumeBlock();
    if (allowedRules == KeyframeRules)
        return nullptr; // Parse error, no at-rules supported inside @keyframes

    ASSERT(allowedRules <= RegularRules);

    if (equalIgnoringCase(name, "media"))
        return consumeMediaRule(prelude, block);
    if (equalIgnoringCase(name, "supports"))
        return consumeSupportsRule(prelude, block);
    if (equalIgnoringCase(name, "viewport"))
        return consumeViewportRule(prelude, block);
    if (equalIgnoringCase(name, "font-face"))
        return consumeFontFaceRule(prelude, block);
    if (equalIgnoringCase(name, "-webkit-keyframes"))
        return consumeKeyframesRule(true, prelude, block);
    if (RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled() && equalIgnoringCase(name, "keyframes"))
        return consumeKeyframesRule(false, prelude, block);
    if (equalIgnoringCase(name, "page"))
        return consumePageRule(prelude, block);
    return nullptr; // Parse error, unrecognised at-rule with block
}

PassRefPtrWillBeRawPtr<StyleRuleBase> CSSParserImpl::consumeQualifiedRule(CSSParserTokenRange& range, AllowedRulesType allowedRules)
{
    const CSSParserToken* preludeStart = &range.peek();
    while (!range.atEnd() && range.peek().type() != LeftBraceToken)
        range.consumeComponentValue();

    if (range.atEnd())
        return nullptr; // Parse error, EOF instead of qualified rule block

    CSSParserTokenRange prelude = range.makeSubRange(preludeStart, &range.peek());
    CSSParserTokenRange block = range.consumeBlock();

    if (allowedRules <= RegularRules)
        return consumeStyleRule(prelude, block);
    if (allowedRules == KeyframeRules)
        return consumeKeyframeStyleRule(prelude, block);

    ASSERT_NOT_REACHED();
    return nullptr;
}

// This may still consume tokens if it fails
static String consumeStringOrURI(CSSParserTokenRange& range)
{
    const CSSParserToken& token = range.peek();

    if (token.type() == StringToken || token.type() == UrlToken)
        return AtomicString(range.consumeIncludingWhitespaceAndComments().value());

    if (token.type() != FunctionToken || !equalIgnoringCase(token.value(), "url"))
        return AtomicString();

    CSSParserTokenRange contents = range.consumeBlock();
    const CSSParserToken& uri = contents.consumeIncludingWhitespaceAndComments();
    ASSERT(uri.type() == StringToken);
    if (!contents.atEnd())
        return AtomicString();
    return AtomicString(uri.value());
}

PassRefPtrWillBeRawPtr<StyleRuleImport> CSSParserImpl::consumeImportRule(CSSParserTokenRange prelude)
{
    prelude.consumeWhitespaceAndComments();
    AtomicString uri(consumeStringOrURI(prelude));
    if (uri.isNull())
        return nullptr; // Parse error, expected string or URI
    return StyleRuleImport::create(uri, MediaQueryParser::parseMediaQuerySet(prelude));
}

PassRefPtrWillBeRawPtr<StyleRuleNamespace> CSSParserImpl::consumeNamespaceRule(CSSParserTokenRange prelude)
{
    prelude.consumeWhitespaceAndComments();
    AtomicString namespacePrefix;
    if (prelude.peek().type() == IdentToken)
        namespacePrefix = AtomicString(prelude.consumeIncludingWhitespaceAndComments().value());

    AtomicString uri(consumeStringOrURI(prelude));
    prelude.consumeWhitespaceAndComments();
    if (uri.isNull() || !prelude.atEnd())
        return nullptr; // Parse error, expected string or URI

    if (namespacePrefix.isNull())
        m_defaultNamespace = uri;
    return StyleRuleNamespace::create(namespacePrefix, uri);
}

PassRefPtrWillBeRawPtr<StyleRuleMedia> CSSParserImpl::consumeMediaRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase>> rules;
    consumeRuleList(block, RegularRuleList, [&rules](PassRefPtrWillBeRawPtr<StyleRuleBase> rule) {
        rules.append(rule);
    });
    return StyleRuleMedia::create(MediaQueryParser::parseMediaQuerySet(prelude), rules);
}

PassRefPtrWillBeRawPtr<StyleRuleSupports> CSSParserImpl::consumeSupportsRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSSupportsParser::SupportsResult supported = CSSSupportsParser::supportsCondition(prelude, *this);
    if (supported == CSSSupportsParser::Invalid)
        return nullptr; // Parse error, invalid @supports condition
    // FIXME: Serialize the condition text for the CSSOM
    WillBeHeapVector<RefPtrWillBeMember<StyleRuleBase>> rules;
    consumeRuleList(block, RegularRuleList, [&rules](PassRefPtrWillBeRawPtr<StyleRuleBase> rule) {
        rules.append(rule);
    });
    return StyleRuleSupports::create(String(""), supported, rules);
}

PassRefPtrWillBeRawPtr<StyleRuleViewport> CSSParserImpl::consumeViewportRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    // Allow @viewport rules from UA stylesheets even if the feature is disabled.
    if (!RuntimeEnabledFeatures::cssViewportEnabled() && !isUASheetBehavior(m_context.mode()))
        return nullptr;

    prelude.consumeWhitespaceAndComments();
    if (!prelude.atEnd())
        return nullptr; // Parser error; @viewport prelude should be empty
    consumeDeclarationList(block, CSSRuleSourceData::VIEWPORT_RULE);
    RefPtrWillBeRawPtr<StyleRuleViewport> rule = StyleRuleViewport::create();
    rule->setProperties(createStylePropertySet(m_parsedProperties, CSSViewportRuleMode));
    m_parsedProperties.clear();
    return rule.release();
}

PassRefPtrWillBeRawPtr<StyleRuleFontFace> CSSParserImpl::consumeFontFaceRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    prelude.consumeWhitespaceAndComments();
    if (!prelude.atEnd())
        return nullptr; // Parse error; @font-face prelude should be empty
    consumeDeclarationList(block, CSSRuleSourceData::FONT_FACE_RULE);

    // FIXME: This logic should be in CSSPropertyParser
    // FIXME: Shouldn't we fail if font-family or src aren't specified?
    for (unsigned i = 0; i < m_parsedProperties.size(); ++i) {
        CSSProperty& property = m_parsedProperties[i];
        if (property.id() == CSSPropertyFontVariant && property.value()->isPrimitiveValue()) {
            property.wrapValueInCommaSeparatedList();
        } else if (property.id() == CSSPropertyFontFamily && (!property.value()->isValueList() || toCSSValueList(property.value())->length() != 1)) {
            m_parsedProperties.clear();
            return nullptr;
        }
    }

    RefPtrWillBeRawPtr<StyleRuleFontFace> rule = StyleRuleFontFace::create();
    rule->setProperties(createStylePropertySet(m_parsedProperties, m_context.mode()));
    m_parsedProperties.clear();
    if (m_styleSheet)
        m_styleSheet->setHasFontFaceRule(true);
    return rule.release();
}

PassRefPtrWillBeRawPtr<StyleRuleKeyframes> CSSParserImpl::consumeKeyframesRule(bool webkitPrefixed, CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    prelude.consumeWhitespaceAndComments();
    const CSSParserToken& nameToken = prelude.consumeIncludingWhitespaceAndComments();
    if (!prelude.atEnd())
        return nullptr; // Parse error; expected single non-whitespace token in @keyframes header

    String name;
    if (nameToken.type() == IdentToken) {
        name = nameToken.value();
    } else if (nameToken.type() == StringToken && webkitPrefixed) {
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::QuotedKeyframesRule);
        name = nameToken.value();
    } else {
        return nullptr; // Parse error; expected ident token in @keyframes header
    }

    RefPtrWillBeRawPtr<StyleRuleKeyframes> keyframeRule = StyleRuleKeyframes::create();
    consumeRuleList(block, KeyframesRuleList, [keyframeRule](PassRefPtrWillBeRawPtr<StyleRuleBase> keyframe) {
        keyframeRule->parserAppendKeyframe(toStyleRuleKeyframe(keyframe.get()));
    });
    keyframeRule->setName(name);
    keyframeRule->setVendorPrefixed(webkitPrefixed);
    return keyframeRule.release();
}

PassRefPtrWillBeRawPtr<StyleRulePage> CSSParserImpl::consumePageRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    // We only support a small subset of the css-page spec.
    prelude.consumeWhitespaceAndComments();
    AtomicString typeSelector;
    if (prelude.peek().type() == IdentToken)
        typeSelector = AtomicString(prelude.consumeIncludingComments().value());

    AtomicString pseudo;
    if (prelude.peek().type() == ColonToken) {
        prelude.consumeIncludingComments();
        if (prelude.peek().type() != IdentToken)
            return nullptr; // Parse error; expected ident token following colon in @page header
        pseudo = AtomicString(prelude.consumeIncludingComments().value());
    }

    prelude.consumeWhitespaceAndComments();
    if (!prelude.atEnd())
        return nullptr; // Parse error; extra tokens in @page header

    OwnPtr<CSSParserSelector> selector;
    if (!typeSelector.isNull() && pseudo.isNull()) {
        selector = CSSParserSelector::create(QualifiedName(nullAtom, typeSelector, m_defaultNamespace));
    } else {
        selector = CSSParserSelector::create();
        if (!pseudo.isNull()) {
            selector->setMatch(CSSSelector::PagePseudoClass);
            selector->setValue(pseudo.lower());
            if (selector->pseudoType() == CSSSelector::PseudoUnknown)
                return nullptr; // Parse error; unknown page pseudo-class
        }
        if (!typeSelector.isNull())
            selector->prependTagSelector(QualifiedName(nullAtom, typeSelector, m_defaultNamespace));
    }

    selector->setForPage();

    RefPtrWillBeRawPtr<StyleRulePage> pageRule = StyleRulePage::create();
    Vector<OwnPtr<CSSParserSelector>> selectorVector;
    selectorVector.append(selector.release());
    pageRule->parserAdoptSelectorVector(selectorVector);

    consumeDeclarationList(block, CSSRuleSourceData::STYLE_RULE);
    pageRule->setProperties(createStylePropertySet(m_parsedProperties, m_context.mode()));
    m_parsedProperties.clear();

    return pageRule.release();
}

PassRefPtrWillBeRawPtr<StyleRuleKeyframe> CSSParserImpl::consumeKeyframeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    OwnPtr<Vector<double>> keyList = consumeKeyframeKeyList(prelude);
    if (!keyList)
        return nullptr;
    consumeDeclarationList(block, CSSRuleSourceData::KEYFRAMES_RULE);
    RefPtrWillBeRawPtr<StyleRuleKeyframe> rule = StyleRuleKeyframe::create();
    rule->setKeys(keyList.release());
    rule->setProperties(createStylePropertySet(m_parsedProperties, m_context.mode()));
    m_parsedProperties.clear();
    return rule.release();
}

PassRefPtrWillBeRawPtr<StyleRule> CSSParserImpl::consumeStyleRule(CSSParserTokenRange prelude, CSSParserTokenRange block)
{
    CSSSelectorList selectorList;
    CSSSelectorParser::parseSelector(prelude, m_context, m_defaultNamespace, m_styleSheet, selectorList);
    if (!selectorList.isValid())
        return nullptr; // Parse error, invalid selector list
    consumeDeclarationList(block, CSSRuleSourceData::STYLE_RULE);

    RefPtrWillBeRawPtr<StyleRule> rule = StyleRule::create();
    rule->wrapperAdoptSelectorList(selectorList);
    rule->setProperties(createStylePropertySet(m_parsedProperties, m_context.mode()));
    m_parsedProperties.clear();
    return rule.release();
}

void CSSParserImpl::consumeDeclarationList(CSSParserTokenRange range, CSSRuleSourceData::Type ruleType)
{
    ASSERT(m_parsedProperties.isEmpty());

    while (!range.atEnd()) {
        switch (range.peek().type()) {
        case CommentToken:
        case WhitespaceToken:
        case SemicolonToken:
            range.consume();
            break;
        case IdentToken: {
            const CSSParserToken* declarationStart = &range.peek();
            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();
            consumeDeclaration(range.makeSubRange(declarationStart, &range.peek()), ruleType);
            break;
        }
        default: // Parser error
            // FIXME: The spec allows at-rules in a declaration list
            while (!range.atEnd() && range.peek().type() != SemicolonToken)
                range.consumeComponentValue();
            break;
        }
    }
}

void CSSParserImpl::consumeDeclaration(CSSParserTokenRange range, CSSRuleSourceData::Type ruleType)
{
    ASSERT(range.peek().type() == IdentToken);
    CSSPropertyID id = range.consumeIncludingWhitespaceAndComments().parseAsCSSPropertyID();
    if (id == CSSPropertyInvalid)
        return;
    if (range.consume().type() != ColonToken)
        return; // Parse error

    // FIXME: We shouldn't allow !important in @keyframes or @font-face
    const CSSParserToken* last = range.end() - 1;
    while (last->type() == WhitespaceToken || last->type() == CommentToken)
        --last;
    if (last->type() == IdentToken && equalIgnoringCase(last->value(), "important")) {
        --last;
        while (last->type() == WhitespaceToken || last->type() == CommentToken)
            --last;
        if (last->type() == DelimiterToken && last->delimiter() == '!') {
            consumeDeclarationValue(range.makeSubRange(&range.peek(), last), id, true, ruleType);
            return;
        }
    }
    consumeDeclarationValue(range.makeSubRange(&range.peek(), range.end()), id, false, ruleType);
}

void CSSParserImpl::consumeDeclarationValue(CSSParserTokenRange range, CSSPropertyID propertyID, bool important, CSSRuleSourceData::Type ruleType)
{
    CSSParserValueList valueList(range);
    if (!valueList.size())
        return; // Parser error
    bool inViewport = ruleType == CSSRuleSourceData::VIEWPORT_RULE;
    CSSPropertyParser::parseValue(propertyID, important, &valueList, m_context, inViewport, m_parsedProperties, ruleType);
}

PassOwnPtr<Vector<double>> CSSParserImpl::consumeKeyframeKeyList(CSSParserTokenRange range)
{
    OwnPtr<Vector<double>> result = adoptPtr(new Vector<double>);
    while (true) {
        range.consumeWhitespaceAndComments();
        const CSSParserToken& token = range.consumeIncludingWhitespaceAndComments();
        if (token.type() == PercentageToken && token.numericValue() >= 0 && token.numericValue() <= 100)
            result->append(token.numericValue() / 100);
        else if (token.type() == IdentToken && equalIgnoringCase(token.value(), "from"))
            result->append(0);
        else if (token.type() == IdentToken && equalIgnoringCase(token.value(), "to"))
            result->append(1);
        else
            return nullptr; // Parser error, invalid value in keyframe selector
        if (range.atEnd())
            return result.release();
        if (range.consume().type() != CommaToken)
            return nullptr; // Parser error
    }
}

} // namespace blink
