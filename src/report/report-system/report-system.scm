;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;  report-system.scm
;;  module definition for the report system code 
;;
;;  Copyright (c) 2001 Linux Developers Group, Inc. 
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

(define-module (gnucash report report-system))

(use-modules (ice-9 slib))
(use-modules (ice-9 regex))
(use-modules (srfi srfi-1))
(use-modules (srfi srfi-19))
(use-modules (gnucash gnc-module))

(require 'hash-table)

(gnc:module-load "gnucash/engine" 0)
(gnc:module-load "gnucash/app-utils" 0)

;; commodity-utilities.scm
(export gnc:commodity-is-currency?)
(export gnc:get-match-commodity-splits)
(export gnc:get-match-commodity-splits-sorted)
(export gnc:get-all-commodity-splits )
(export gnc:commodity-numeric->string)
(export gnc:exchange-by-euro-numeric)
(export gnc:get-commodity-totalavg-prices)
(export gnc:get-commoditylist-totalavg-prices)
(export gnc:get-commodity-inst-prices)
(export gnc:get-commoditylist-inst-prices)
(export gnc:pricelist-price-find-nearest)
(export gnc:pricealist-lookup-nearest-in-time)
(export gnc:resolve-unknown-comm)
(export gnc:get-exchange-totals)
(export gnc:make-exchange-alist)
(export gnc:exchange-by-euro)
(export gnc:exchange-if-same)
(export gnc:make-exchange-function)
(export gnc:exchange-by-pricevalue-helper)
(export gnc:exchange-by-pricedb-helper)
(export gnc:exchange-by-pricedb-latest )
(export gnc:exchange-by-pricedb-nearest)
(export gnc:exchange-by-pricealist-nearest)
(export gnc:case-exchange-fn)
(export gnc:case-exchange-time-fn)
(export gnc:sum-collector-commodity)
(export gnc:sum-collector-stocks)


;; options-utilities.scm 

(export gnc:options-add-report-date!)
(export gnc:options-add-date-interval!)
(export gnc:options-add-interval-choice!)
(export gnc:options-add-account-levels!)
(export gnc:options-add-account-selection!)
(export gnc:options-add-include-subaccounts!)
(export gnc:options-add-group-accounts!)
(export gnc:options-add-currency!)
(export gnc:options-add-currency-selection!)
(export gnc:options-add-price-source!)
(export gnc:options-add-plot-size!)
(export gnc:options-add-marker-choice!)

;; html-utilities.scm 

(export gnc:html-make-empty-cells)
(export gnc:account-anchor-text)
(export gnc:split-anchor-text)
(export gnc:transaction-anchor-text)
(export gnc:report-anchor-text)
(export gnc:make-report-anchor)
(export gnc:html-account-anchor)
(export gnc:html-split-anchor)
(export gnc:html-transaction-anchor)
(export gnc:assign-colors)
(export gnc:html-table-append-ruler!)
(export gnc:html-table-append-ruler/markup!)
(export gnc:html-acct-table-cell)
(export gnc:html-acct-table-row-helper! )
(export gnc:html-acct-table-comm-row-helper!)
(export gnc:html-build-acct-table )
(export gnc:html-make-exchangerates)
(export gnc:html-make-no-account-warning)
(export gnc:html-make-empty-data-warning)

;; report.scm
(export gnc:menuname-reports)
(export gnc:menuname-asset-liability)
(export gnc:menuname-income-expense )
(export gnc:menuname-taxes)
(export gnc:menuname-utility)
(export gnc:pagename-general)
(export gnc:pagename-accounts)
(export gnc:pagename-display)
(export gnc:optname-reportname)

(export gnc:define-report)
(export <report>)
(export *gnc:_reports_*)
(export gnc:report-template-new-options/name)
(export gnc:report-template-menu-name/name)
(export gnc:report-template-new-options)
(export gnc:report-template-version)
(export gnc:report-template-name)
(export gnc:report-template-options-generator)
(export gnc:report-template-options-editor)
(export gnc:report-template-options-cleanup-cb)
(export gnc:report-template-options-changed-cb)
(export gnc:report-template-renderer)
(export gnc:report-template-in-menu?)
(export gnc:report-template-menu-path)
(export gnc:report-template-menu-name)
(export gnc:report-template-menu-tip)
(export gnc:report-template-export-thunk)
(export gnc:report-type)
(export gnc:report-set-type!)
(export gnc:report-id)
(export gnc:report-set-id!)
(export gnc:report-options)
(export gnc:report-set-options!)
(export gnc:report-needs-save?)
(export gnc:report-set-needs-save?!)
(export gnc:report-dirty?)
(export gnc:report-set-dirty?!)
(export gnc:report-editor-widget)
(export gnc:report-set-editor-widget!)
(export gnc:report-ctext)
(export gnc:report-set-ctext!)
(export gnc:report-edit-options)
(export gnc:make-report)
(export gnc:restore-report)
(export gnc:make-report-options)
(export gnc:report-options-editor)
(export gnc:report-export-thunk)
(export gnc:report-menu-name)
(export gnc:report-name)
(export gnc:report-stylesheet)
(export gnc:report-set-stylesheet!)
(export gnc:all-report-template-names)
(export gnc:report-remove-by-id)
(export gnc:find-report)
(export gnc:report-generate-restore-forms)
(export gnc:report-render-html)
(export gnc:report-run)
(export gnc:report-templates-for-each)

;; html-barchart.scm

(export <html-barchart>)
(export gnc:html-barchart? )
(export gnc:make-html-barchart-internal)
(export gnc:make-html-barchart)
(export gnc:html-barchart-data)
(export gnc:html-barchart-set-data!)
(export gnc:html-barchart-width)
(export gnc:html-barchart-set-width!)
(export gnc:html-barchart-height)
(export gnc:html-barchart-set-height!)
(export gnc:html-barchart-x-axis-label)
(export gnc:html-barchart-set-x-axis-label!)
(export gnc:html-barchart-y-axis-label)
(export gnc:html-barchart-set-y-axis-label!)
(export gnc:html-barchart-row-labels)
(export gnc:html-barchart-set-row-labels!)
(export gnc:html-barchart-row-labels-rotated?)
(export gnc:html-barchart-set-row-labels-rotated?!)
(export gnc:html-barchart-stacked?)
(export gnc:html-barchart-set-stacked?!)
(export gnc:html-barchart-col-labels)
(export gnc:html-barchart-set-col-labels!)
(export gnc:html-barchart-col-colors)
(export gnc:html-barchart-set-col-colors!)
(export gnc:html-barchart-legend-reversed?)
(export gnc:html-barchart-set-legend-reversed?!)
(export gnc:html-barchart-title)
(export gnc:html-barchart-set-title!)
(export gnc:html-barchart-subtitle)
(export gnc:html-barchart-set-subtitle!)
(export gnc:html-barchart-button-1-bar-urls)
(export gnc:html-barchart-set-button-1-bar-urls!)
(export gnc:html-barchart-button-2-bar-urls)
(export gnc:html-barchart-set-button-2-bar-urls!)
(export gnc:html-barchart-button-3-bar-urls)
(export gnc:html-barchart-set-button-3-bar-urls!)
(export gnc:html-barchart-button-1-legend-urls)
(export gnc:html-barchart-set-button-1-legend-urls!)
(export gnc:html-barchart-button-2-legend-urls)
(export gnc:html-barchart-set-button-2-legend-urls!)
(export gnc:html-barchart-button-3-legend-urls)
(export gnc:html-barchart-set-button-3-legend-urls!)
(export gnc:html-barchart-append-row!)
(export gnc:html-barchart-prepend-row!)
(export gnc:html-barchart-append-column!)
(export gnc:not-all-zeros)
(export gnc:html-barchart-prepend-column!)
(export gnc:html-barchart-render barchart)

;; html-document.scm

(export <html-document>)
(export gnc:html-document?)
(export gnc:make-html-document-internal)
(export gnc:make-html-document)
(export gnc:html-document-set-title!)
(export gnc:html-document-title)
(export gnc:html-document-set-style-sheet!)
(export gnc:html-document-style-sheet)
(export gnc:html-document-set-style-stack!)
(export gnc:html-document-style-stack)
(export gnc:html-document-set-style-internal!)
(export gnc:html-document-style)
(export gnc:html-document-set-objects!)
(export gnc:html-document-objects)
(export gnc:html-document?)
(export gnc:html-document-set-style!)
(export gnc:html-document-tree-collapse)
(export gnc:html-document-render)
(export gnc:html-document-push-style)
(export gnc:html-document-pop-style)
(export gnc:html-document-add-object!)
(export gnc:html-document-append-objects!)
(export gnc:html-document-fetch-markup-style)
(export gnc:html-document-fetch-data-style)
(export gnc:html-document-markup-start)
(export gnc:html-document-markup-end)
(export gnc:html-document-render-data)
(export <html-object>)
(export gnc:html-object?)
(export gnc:make-html-object-internal)
(export gnc:make-html-object)
(export gnc:html-object-renderer)
(export gnc:html-object-set-renderer!)
(export gnc:html-object-data)
(export gnc:html-object-set-data!)
(export gnc:html-object-render)

;; html-piechart.scm

(export <html-piechart>)
(export gnc:html-piechart?)
(export gnc:make-html-piechart-internal)
(export gnc:make-html-piechart)
(export gnc:html-piechart-data)
(export gnc:html-piechart-set-data!)
(export gnc:html-piechart-width)
(export gnc:html-piechart-set-width!)
(export gnc:html-piechart-height)
(export gnc:html-piechart-set-height!)
(export gnc:html-piechart-labels)
(export gnc:html-piechart-set-labels!)
(export gnc:html-piechart-colors)
(export gnc:html-piechart-set-colors!)
(export gnc:html-piechart-title)
(export gnc:html-piechart-set-title!)
(export gnc:html-piechart-subtitle)
(export gnc:html-piechart-set-subtitle!)
(export gnc:html-piechart-button-1-slice-urls)
(export gnc:html-piechart-set-button-1-slice-urls!)
(export gnc:html-piechart-button-2-slice-urls)
(export gnc:html-piechart-set-button-2-slice-urls!)
(export gnc:html-piechart-button-3-slice-urls)
(export gnc:html-piechart-set-button-3-slice-urls!)
(export gnc:html-piechart-button-1-legend-urls)
(export gnc:html-piechart-set-button-1-legend-urls!)
(export gnc:html-piechart-button-2-legend-urls)
(export gnc:html-piechart-set-button-2-legend-urls!)
(export gnc:html-piechart-button-3-legend-urls)
(export gnc:html-piechart-set-button-3-legend-urls!)
(export gnc:html-piechart-render)

;; html-scatter.scm

(export <html-scatter>)
(export gnc:html-scatter?)
(export gnc:make-html-scatter-internal)
(export gnc:make-html-scatter)
(export gnc:html-scatter-width)
(export gnc:html-scatter-set-width!)
(export gnc:html-scatter-height)
(export gnc:html-scatter-set-height!)
(export gnc:html-scatter-title)
(export gnc:html-scatter-set-title!)
(export gnc:html-scatter-subtitle)
(export gnc:html-scatter-set-subtitle!)
(export gnc:html-scatter-x-axis-label)
(export gnc:html-scatter-set-x-axis-label!)
(export gnc:html-scatter-y-axis-label)
(export gnc:html-scatter-set-y-axis-label!)
(export gnc:html-scatter-data)
(export gnc:html-scatter-set-data!)
(export gnc:html-scatter-marker)
(export gnc:html-scatter-set-marker!)
(export gnc:html-scatter-markercolor)
(export gnc:html-scatter-set-markercolor!)
(export gnc:html-scatter-add-datapoint!)
(export gnc:html-scatter-render)

;; html-style-info.scm 

(export make-kvtable)
(export kvt-ref)
(export kvt-set!)
(export kvt-fold)
(export <html-markup-style-info>)
(export gnc:html-markup-style-info?)
(export gnc:make-html-markup-style-info-internal)
(export gnc:make-html-markup-style-info)
(export gnc:html-markup-style-info-set!)
(export gnc:html-markup-style-info-tag)
(export gnc:html-markup-style-info-set-tag!)
(export gnc:html-markup-style-info-attributes)
(export gnc:html-markup-style-info-set-attributes!)
(export gnc:html-markup-style-info-font-face)
(export gnc:html-markup-style-info-set-font-face-internal!)
(export gnc:html-markup-style-info-set-font-face!)
(export gnc:html-markup-style-info-font-size)
(export gnc:html-markup-style-info-set-font-size-internal!)
(export gnc:html-markup-style-info-set-font-size!)
(export gnc:html-markup-style-info-font-color)
(export gnc:html-markup-style-info-set-font-color-internal!)
(export gnc:html-markup-style-info-set-font-color!)
(export gnc:html-markup-style-info-closing-font-tag)
(export gnc:html-markup-style-info-set-closing-font-tag!)
(export gnc:html-markup-style-info-inheritable?)
(export gnc:html-markup-style-info-set-inheritable?!)
(export gnc:html-markup-style-info-set-attribute!)
(export gnc:html-markup-style-info-merge)
(export gnc:html-style-info-merge)
(export gnc:html-data-style-info-merge)
(export <html-data-style-info>)
(export gnc:html-data-style-info?)
(export gnc:make-html-data-style-info-internal)
(export gnc:make-html-data-style-info)
(export gnc:html-data-style-info?)
(export gnc:html-data-style-info-renderer)
(export gnc:html-data-style-info-set-renderer!)
(export gnc:html-data-style-info-data)
(export gnc:html-data-style-info-set-data!)
(export gnc:html-data-style-info-inheritable?)
(export gnc:html-data-style-info-set-inheritable?!)
(export gnc:default-html-string-renderer)
(export gnc:default-html-gnc-numeric-renderer)
(export gnc:default-html-gnc-monetary-renderer)
(export gnc:default-html-number-renderer)
(export <html-style-table>)
(export gnc:html-style-table?)
(export gnc:make-html-style-table-internal)
(export gnc:make-html-style-table)
(export gnc:html-style-table-primary)
(export gnc:html-style-table-compiled)
(export gnc:html-style-table-set-compiled!)
(export gnc:html-style-table-inheritable)
(export gnc:html-style-table-set-inheritable!)
(export gnc:html-style-table-compiled?)
(export gnc:html-style-table-compile)
(export gnc:html-style-table-uncompile)
(export gnc:html-style-table-fetch)
(export gnc:html-style-table-set!)

;; html-style-sheet.scm

(export <html-style-sheet-template>)
(export gnc:html-style-sheet-template?)
(export gnc:html-style-sheet-template-version)
(export gnc:html-style-sheet-template-set-version!)
(export gnc:html-style-sheet-template-name)
(export gnc:html-style-sheet-template-set-name!)
(export gnc:html-style-sheet-template-options-generator)
(export gnc:html-style-sheet-template-set-options-generator!)
(export gnc:html-style-sheet-template-renderer)
(export gnc:html-style-sheet-template-set-renderer!)
(export gnc:html-style-sheet-template-find)
(export gnc:define-html-style-sheet)
(export <html-style-sheet>)
(export gnc:html-style-sheet?)
(export gnc:html-style-sheet-name)
(export gnc:html-style-sheet-set-name!)
(export gnc:html-style-sheet-type)
(export gnc:html-style-sheet-set-type!)
(export gnc:html-style-sheet-options)
(export gnc:html-style-sheet-set-options!)
(export gnc:html-style-sheet-renderer)
(export gnc:html-style-sheet-set-renderer!)
(export gnc:make-html-style-sheet-internal)
(export gnc:html-style-sheet-style)
(export gnc:html-style-sheet-set-style!)
(export gnc:make-html-style-sheet)
(export gnc:restore-html-style-sheet)
(export gnc:html-style-sheet-apply-changes)
(export gnc:html-style-sheet-render)
(export gnc:get-html-style-sheets)
(export gnc:get-html-templates)
(export gnc:html-style-sheet-find)
(export gnc:html-style-sheet-remove)

;; html-table.scm 

(export <html-table>)
(export gnc:html-table?)
(export <html-table-cell>)
(export gnc:make-html-table-cell-internal)
(export gnc:make-html-table-cell)
(export gnc:make-html-table-cell/size)
(export gnc:make-html-table-cell/markup)
(export gnc:make-html-table-cell/size/markup)
(export gnc:make-html-table-header-cell)
(export gnc:make-html-table-header-cell/markup)
(export gnc:make-html-table-header-cell/size)
(export gnc:html-table-cell?)
(export gnc:html-table-cell-rowspan)
(export gnc:html-table-cell-set-rowspan!)
(export gnc:html-table-cell-colspan)
(export gnc:html-table-cell-set-colspan!)
(export gnc:html-table-cell-tag)
(export gnc:html-table-cell-set-tag!)
(export gnc:html-table-cell-data)
(export gnc:html-table-cell-set-data-internal!)
(export gnc:html-table-cell-style)
(export gnc:html-table-cell-set-style-internal!)
(export gnc:html-table-cell-set-style!)
(export gnc:html-table-cell-append-objects!)
(export gnc:html-table-cell-render)
(export gnc:make-html-table-internal)
(export gnc:make-html-table)
(export gnc:html-table-data)
(export gnc:html-table-set-data!)
(export gnc:html-table-caption)
(export gnc:html-table-set-caption!)
(export gnc:html-table-col-headers)
(export gnc:html-table-set-col-headers!)
(export gnc:html-table-row-headers)
(export gnc:html-table-set-row-headers!)
(export gnc:html-table-style)
(export gnc:html-table-set-style-internal!)
(export gnc:html-table-row-styles)
(export gnc:html-table-set-row-styles!)
(export gnc:html-table-row-markup-table)
(export gnc:html-table-row-markup)
(export gnc:html-table-set-row-markup-table!)
(export gnc:html-table-set-row-markup!)
(export gnc:html-table-col-styles)
(export gnc:html-table-set-col-styles!)
(export gnc:html-table-col-headers-style)
(export gnc:html-table-set-col-headers-style!)
(export gnc:html-table-row-headers-style)
(export gnc:html-table-set-row-headers-style!)
(export gnc:html-table-set-style!)
(export gnc:html-table-set-col-style!)
(export gnc:html-table-set-row-style!)
(export gnc:html-table-row-style)
(export gnc:html-table-col-style)
(export gnc:html-table-num-rows)
(export gnc:html-table-set-num-rows-internal!)
(export gnc:html-table-num-columns)
(export gnc:html-table-append-row/markup!)
(export gnc:html-table-prepend-row/markup!)
(export gnc:html-table-append-row!)
(export gnc:html-table-remove-last-row!)
(export gnc:html-table-prepend-row!)
(export gnc:html-table-set-cell!)
(export gnc:html-table-append-column!)
(export gnc:html-table-prepend-column!)
(export gnc:html-table-render)

;; html-text.scm

(export <html-text>)
(export gnc:html-text?)
(export gnc:make-html-text-internal)
(export gnc:make-html-text)
(export gnc:html-text?)
(export gnc:html-text-body)
(export gnc:html-text-set-body-internal!)
(export gnc:html-text-set-body!)
(export gnc:html-text-style)
(export gnc:html-text-set-style-internal!)
(export gnc:html-text-set-style!)
(export gnc:html-text-append!)
(export gnc:html-markup)
(export gnc:html-markup/attr)
(export gnc:html-markup/no-end)
(export gnc:html-markup/attr/no-end)
(export gnc:html-markup/format)
(export gnc:html-markup-p)
(export gnc:html-markup-tt)
(export gnc:html-markup-em)
(export gnc:html-markup-b)
(export gnc:html-markup-i)
(export gnc:html-markup-h1)
(export gnc:html-markup-h2)
(export gnc:html-markup-h3)
(export gnc:html-markup-br)
(export gnc:html-markup-hr)
(export gnc:html-markup-ul)
(export gnc:html-markup-anchor)
(export gnc:html-markup-img)
(export gnc:html-text-render)
(export gnc:html-text-render-markup)

;; report-utilities.scm 

(export list-ref-safe)
(export list-set-safe!)
(export gnc:amount->string)
(export gnc:commodity-value->string)
(export gnc:monetary->string)
(export gnc:account-has-shares?)
(export gnc:account-is-stock?)
(export gnc:account-is-inc-exp?)
(export gnc:filter-accountlist-type)
(export gnc:decompose-accountlist)
(export gnc:account-get-type-string-plural)
(export gnc:accounts-get-commodities)
(export gnc:get-current-group-depth)
(export gnc:split-get-corr-account-full-name)
(export gnc:account-get-immediate-subaccounts)
(export gnc:account-get-all-subaccounts)
(export gnc:acccounts-get-all-subaccounts)
(export gnc:transaction-map-splits)
(export gnc:make-stats-collector)
(export gnc:make-drcr-collector)
(export gnc:make-value-collector)
(export gnc:make-numeric-collector)
(export gnc:make-commodity-collector)
(export gnc:account-get-balance-at-date)
(export gnc:account-get-comm-balance-at-date)
(export gnc:accounts-get-balance-helper)
(export gnc:accounts-get-comm-total-profit)
(export gnc:accounts-get-comm-total-income)
(export gnc:accounts-get-comm-total-expense)
(export gnc:accounts-get-comm-total-assets)
(export gnc:group-get-comm-balance-at-date)
(export gnc:account-get-balance-interval)
(export gnc:account-get-comm-balance-interval)
(export gnc:group-get-comm-balance-interval)

(load-from-path "commodity-utilities.scm")
(load-from-path "html-barchart.scm")
(load-from-path "html-document.scm")
(load-from-path "html-piechart.scm")
(load-from-path "html-scatter.scm")
(load-from-path "html-style-info.scm")

(load-from-path "html-style-sheet.scm")
(load-from-path "html-table.scm")
(load-from-path "html-text.scm")
(load-from-path "html-utilities.scm")
(load-from-path "options-utilities.scm")
(load-from-path "report-utilities.scm")
(load-from-path "report.scm")

(gnc:hook-add-dangler gnc:*save-options-hook* gnc:save-style-sheet-options)
